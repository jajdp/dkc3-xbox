// uwp_host.cpp — Host UWP (Xbox Series X, dev mode) para DKC3Recomp.
//
// No usa SDL ni OpenGL. El juego produce un cuadro terminado en CPU (BGRA,
// Dkc3VideoWidth()x224, 16:9 incluido en el propio cuadro); este host lo sube a
// una textura Direct3D 11 y lo dibuja en un triángulo de pantalla completa, que
// es la API que la Xbox sí permite dentro de UWP. Audio por XAudio2, entrada por
// Windows.Gaming.Input. Modelo de app: CoreApplication + IFrameworkView.
//
// Las funciones del motor (compilado como C) se declaran aquí en un bloque
// extern "C"; sus cabeceras no traen guardas de C++, así que no se incluyen.

#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>
#include <winrt/Windows.System.h>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <xaudio2.h>
#include <wrl/client.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <exception>

#include "uwp_shaders.h"  // g_VS[]/g_VS_len y g_PS[]/g_PS_len (precompilados con fxc)

using Microsoft::WRL::ComPtr;
namespace wf = winrt::Windows::Foundation;
namespace wac = winrt::Windows::ApplicationModel::Core;
namespace wam = winrt::Windows::ApplicationModel;
namespace waa = winrt::Windows::ApplicationModel::Activation;
namespace wst = winrt::Windows::Storage;
namespace wuc = winrt::Windows::UI::Core;
namespace wgi = winrt::Windows::Gaming::Input;
namespace wsy = winrt::Windows::System;

// ------------------------- API del motor (C) -------------------------------
struct Snes;
struct RtlGameInfo;
typedef enum Dkc3VideoAspect {
  kDkc3VideoAspectNative = 0,
  kDkc3VideoAspect16x10,
  kDkc3VideoAspect16x9,
} Dkc3VideoAspect;

extern "C" {
uint8_t *Dkc3ReadVerifiedRom(const char *path, size_t *size_out, char *error,
                             size_t error_size);
void Dkc3VideoSetWidescreen(bool enabled);
void Dkc3VideoSetAspect(Dkc3VideoAspect aspect);
int Dkc3VideoWidth(void);
void Dkc3BeginDrawing(uint8_t *pixels, size_t pitch);
void Dkc3DrawPpuFrame(void);
int Dkc3LastLleResult(void);
const RtlGameInfo *Dkc3GameInfo(void);
void RtlRegisterGame(const RtlGameInfo *info);
Snes *SnesInit(const uint8_t *data, int data_size);
bool RtlRunFrame(uint32_t inputs);
void RtlRenderAudio(int16_t *audio_buffer, int samples, int channels);
void RtlSetAudioOutputRate(int freq);
void RtlSetSaveRoot(const char *root);
void RtlReadSram(void);
void RtlWriteSram(void);
const char *RtlSaveRoot(void);
// Serializador de la máquina entera: lo que hace posible volver donde lo dejaste.
bool RtlSaveSnapshot(const char *filename);
bool RtlLoadSnapshot(const char *filename);
extern bool g_fail;
}

// --- Registro de arranque (a LocalState\dkc3_boot.log) para diagnóstico ---
// Serializado: desde que hay hilo de presentación escriben aquí dos hilos, y sin
// cerrojo las líneas se entrelazan (entre ellas van los latidos con los que se mide).
static FILE *g_log = nullptr;
static std::mutex g_log_mtx;
static void Log(const char *m) {
  std::lock_guard<std::mutex> cerrojo(g_log_mtx);
  if (g_log) { fputs(m, g_log); fputc('\n', g_log); fflush(g_log); }
}
static void Logf(const char *fmt, long v) {
  std::lock_guard<std::mutex> cerrojo(g_log_mtx);
  if (g_log) { fprintf(g_log, fmt, v); fputc('\n', g_log); fflush(g_log); }
}

// --- Contador de FPS en pantalla (Recompilaciones, 2026-10-06) ------------------------
// El host sube un búfer BGRA a una textura, así que el contador se dibuja en el propio
// búfer con esta fuente de 5x7. Solo hacen falta los dígitos, el punto y «FPS».
static const unsigned char k_fps_glifos[16][7] = {
  {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},  // 0
  {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},  // 1
  {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},  // 2
  {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},  // 3
  {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},  // 4
  {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},  // 5
  {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},  // 6
  {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},  // 7
  {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},  // 8
  {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},  // 9
  {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},  // .
  {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},  // F
  {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},  // P
  {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},  // S
  {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11},  // x
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // espacio
};

static int FpsIndiceGlifo(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  switch (c) {
    case '.': return 10;
    case 'F': return 11;
    case 'P': return 12;
    case 'S': return 13;
    case 'x': return 14;
    default:  return 15;
  }
}

// Pinta el texto en el búfer BGRA, con una sombra negra alrededor para que se lea sobre
// cualquier fondo del juego.
//
// En DOS pasadas, y no en una: si cada píxel pintase a la vez su centro claro y su contorno
// negro, el contorno de un píxel borraría el centro del vecino y del texto solo quedarían
// puntos sueltos — que es justo lo que se vio en la consola al primer intento.
static void FpsPintaTexto(unsigned char *px, int w, int h, int x0, int y0, const char *s) {
  for (int pasada = 0; pasada < 2; pasada++) {
    const int v = pasada ? 255 : 0;          // 0 = la sombra, 1 = el trazo
    for (int i = 0; s[i]; i++) {
      const unsigned char *g = k_fps_glifos[FpsIndiceGlifo(s[i])];
      const int bx = x0 + i * 6;
      for (int fila = 0; fila < 7; fila++) {
        for (int col = 0; col < 5; col++) {
          if (!(g[fila] & (0x10 >> col))) continue;
          const int d = pasada ? 0 : 1;      // la sombra se extiende un píxel
          for (int dy = -d; dy <= d; dy++) {
            for (int dx = -d; dx <= d; dx++) {
              const int x = bx + col + dx, y = y0 + fila + dy;
              if (x < 0 || y < 0 || x >= w || y >= h) continue;
              unsigned char *p = px + ((size_t)y * w + x) * 4;
              p[0] = p[1] = p[2] = (unsigned char)v;
              p[3] = 255;
            }
          }
        }
      }
    }
  }
}

// --- Reloj de alta resolución (medición de cadencia) ---
static double g_ms_por_tick = 0.0;
static inline long long Ahora() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}
// Ticks transcurridos desde *desde, dejando *desde en «ahora».
static inline long long Ticks(long long *desde) {
  const long long n = Ahora();
  const long long d = n - *desde;
  *desde = n;
  return d;
}
static inline double Ms(long long *desde) { return Ticks(desde) * g_ms_por_tick; }

// Reparto del tiempo de un cuadro, en el hilo del juego (se acumula 60 cuadros y el
// latido lo promedia).
struct Etapas {
  double emu = 0, comp = 0, aud = 0, ent = 0, evt = 0, espera = 0;
};

static const int kVideoHeight = 224;  // kDkc3VideoHeight

// SNES joypad (igual que win32_host.c PollInput)
enum {
  kB = 0x001, kY = 0x002, kSelect = 0x004, kStart = 0x008,
  kUp = 0x010, kDown = 0x020, kLeft = 0x040, kRight = 0x080,
  kA = 0x100, kX = 0x200, kL = 0x400, kR = 0x800,
};

// ------------------------- Audio (XAudio2) ---------------------------------
struct VoiceCallback : public IXAudio2VoiceCallback {
  void STDMETHODCALLTYPE OnBufferEnd(void *ctx) noexcept override {
    delete[] static_cast<int16_t *>(ctx);
  }
  void STDMETHODCALLTYPE OnBufferStart(void *) noexcept override {}
  void STDMETHODCALLTYPE OnLoopEnd(void *) noexcept override {}
  void STDMETHODCALLTYPE OnStreamEnd() noexcept override {}
  void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) noexcept override {}
  void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() noexcept override {}
  void STDMETHODCALLTYPE OnVoiceError(void *, HRESULT) noexcept override {}
};

// ------------------------- El host -----------------------------------------
struct App : winrt::implements<App, wac::IFrameworkViewSource, wac::IFrameworkView> {
  // D3D
  ComPtr<ID3D11Device> device_;
  ComPtr<ID3D11DeviceContext> ctx_;
  ComPtr<IDXGISwapChain1> swap_;
  ComPtr<ID3D11RenderTargetView> rtv_;
  ComPtr<ID3D11Texture2D> tex_;
  ComPtr<ID3D11ShaderResourceView> srv_;
  ComPtr<ID3D11SamplerState> samp_;
  ComPtr<ID3D11VertexShader> vs_;
  ComPtr<ID3D11PixelShader> ps_;
  UINT bbw_ = 1920, bbh_ = 1080;

  // Audio
  IXAudio2 *xaudio_ = nullptr;
  IXAudio2MasteringVoice *master_ = nullptr;
  IXAudio2SourceVoice *voice_ = nullptr;
  VoiceCallback audio_cb_;
  static const int kAudioRate = 48000;
  static const int kAudioChannels = 2;

  // Juego
  int fw_ = 342;  // Dkc3VideoWidth() (16:9)
  std::string frame_;  // fw_*224*4 BGRA
  bool booted_ = false;
  bool running_ = true;
  bool visible_ = true;

  wuc::CoreWindow window_{nullptr};
  wuc::SystemNavigationManager nav_{nullptr};
  winrt::event_token nav_token_{};

  // ---------------------- Partida en pila (SRAM) --------------------------
  //
  // El motor guarda la pila del cartucho en `<raiz>\save.srm`, pero solo cuando
  // alguien se lo pide: `RtlReadSram()` al arrancar y `RtlWriteSram()` para
  // volcarla. El host de escritorio llama a las dos; este solo fijaba la carpeta
  // con `RtlSetSaveRoot()` y no llamaba a ninguna, asi que la partida ni se
  // cargaba ni se guardaba: lo guardado en el barril de Candy se perdia entero
  // al cerrar el juego. En la consola no hay un «salir» limpio —el shell suspende
  // la app y luego la mata—, asi que el momento bueno para escribir es
  // `Suspending`, que es lo ultimo que la app recibe con vida.
  //
  // El juego escribe su SRAM dentro de `RtlRunFrame()`, asi que solo es seguro
  // volcarla en un borde de cuadro. Si `Suspending` llega por el hilo del bucle
  // (dentro de `ProcessEvents`) ya estamos en un borde y se escribe en el acto;
  // si llega por otro hilo se le pide al bucle que lo haga y se espera, con un
  // segundo de tope por si el bucle no esta corriendo.
  std::thread::id hilo_bucle_{};
  std::atomic<bool> pide_guardar_{false};
  std::atomic<unsigned> guardados_{0};
  winrt::event_token suspend_token_{};

  void EscribeSram(const char *motivo) {
    RtlWriteSram();
    guardados_.fetch_add(1);
    std::string m = "SRAM escrita (";
    m += motivo;
    m += ")";
    Log(m.c_str());
  }

  void GuardaAlSuspender() {
    if (!booted_) { Log("Suspending: sin juego arrancado, nada que guardar"); return; }
    if (std::this_thread::get_id() == hilo_bucle_) {
      EscribeSram("suspension, en el hilo del bucle");
      GuardaEstado("suspension, en el hilo del bucle");
      return;
    }
    const unsigned antes = guardados_.load();
    pide_guardar_.store(true);
    for (int i = 0; i < 200 && guardados_.load() == antes; ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (guardados_.load() == antes) {
      Log("Suspending: el bucle no contesto en 1 s, se escribe desde aqui");
      EscribeSram("suspension, sin el bucle");
    }
  }

  // ---------------------- Volver donde lo dejaste (suspensión) ------------------
  //
  // En Dev Mode la Xbox NO suspende los juegos: los TERMINA en cuanto pasan al
  // fondo (el presupuesto de segundo plano son 128 MB) y al volver se relanzan
  // desde cero, con `PreviousExecutionState = Terminated`. No hay Quick Resume
  // que valga y nadie restaura nada por nosotros; es cosa de la app.
  //
  // El motor sí sabe serializar la máquina entera —`RtlSaveSnapshot` y
  // `RtlLoadSnapshot`, el mismo formato de los estados guardados—, así que basta
  // con escribir uno y volver a cargarlo al relanzar.
  //
  // **Cuándo escribirlo: cada 10 segundos, no en el aviso de suspensión.** Medido
  // en Street Fighter EX2 Plus (2026-10-06), cuando ese aviso llega puede que no
  // se emule ni un cuadro más, y el estado solo es coherente en un borde de
  // cuadro. Se guarda también al suspender, por si llega a tiempo, pero sin
  // depender de ello.
  //
  // **Salvaguarda:** el archivo se aparta antes de cargarlo y se borra después.
  // Si un estado corrupto colgara el juego al restaurarlo, el arranque siguiente
  // ya no lo encuentra y el juego entra limpio, en vez de quedarse inservible.
  std::string estado_ruta_;
  std::atomic<int> prev_exec_{-1};   // PreviousExecutionState; 3 = Terminated
  bool estado_resuelto_ = false;
  unsigned cuadros_resolver_ = 0;
  long long t_estado_ = 0;

  void GuardaEstado(const char *motivo) {
    if (!booted_ || estado_ruta_.empty()) return;
    EstadoEsDeOtraVersion();   /* deja la marca de ESTE binario junto al estado */
    const bool ok = RtlSaveSnapshot(estado_ruta_.c_str());
    std::string m = ok ? "suspension: estado guardado (" : "suspension: NO se pudo guardar el estado (";
    m += motivo;
    m += ")";
    Log(m.c_str());
  }

  // Estado de otra version: un estado guardado por OTRO binario no se puede restaurar — la
  // memoria emulada queda inconsistente y el juego sale con los gráficos rotos (le pasó al
  // usuario en Super Mario World el 2026-10-06). La marca es la RUTA del ejecutable, que en
  // UWP lleva dentro la versión del paquete, así que cambia en cada instalación.
  // No vale la fecha de compilación: solo cambia si se recompila ESE archivo.
  bool EstadoEsDeOtraVersion() {
    char propio[512] = { 0 };
    GetModuleFileNameA(nullptr, propio, (unsigned long)(sizeof propio - 1));
    const std::string ruta_marca = estado_ruta_ + ".marca";
    std::string leido;
    if (FILE *f = fopen(ruta_marca.c_str(), "rb")) {
      char buf[512] = { 0 };
      const size_t n = fread(buf, 1, sizeof buf - 1, f);
      buf[n] = 0;
      leido = buf;
      fclose(f);
    }
    if (leido == propio) return false;
    if (!leido.empty()) Log("suspension: el estado es de otra version, se descarta");
    if (FILE *f = fopen(ruta_marca.c_str(), "wb")) {
      fwrite(propio, 1, strlen(propio), f);
      fclose(f);
    }
    return true;
  }

  void RestauraEstado() {
    if (estado_ruta_.empty()) return;
    if (EstadoEsDeOtraVersion()) {
      DeleteFileA(estado_ruta_.c_str());
      return;
    }
    const std::string apartado = estado_ruta_ + ".cargando";
    DeleteFileA(apartado.c_str());
    if (!MoveFileA(estado_ruta_.c_str(), apartado.c_str())) {
      Log("suspension: la consola nos termino, pero no hay estado guardado");
      return;
    }
    const bool ok = RtlLoadSnapshot(apartado.c_str());
    DeleteFileA(apartado.c_str());
    Log(ok ? "suspension: estado restaurado, sigue la partida"
           : "suspension: el estado no se pudo cargar, se empieza de cero");
  }

  // ------------------- Hilo de presentación y medición --------------------
  //
  // El bucle hacía todo seguido: emular, componer, audio, subir la textura y
  // `Present(1,0)`, que espera al vertical. Con eso el trabajo del cuadro y la
  // espera van en serie: si lo que cuesta emular + componer + presentar pasa de
  // los 16,7 ms de un vertical, se pierde el siguiente y la simulación cae a la
  // mitad. Aquí el cuadro terminado se copia a un búfer de entrega y lo dibuja un
  // hilo propio, que es **el único que toca el contexto de Direct3D 11** y el
  // único que espera al vertical; el juego solo espera a que ese hilo se lleve el
  // cuadro anterior, así que el ritmo lo sigue marcando la pantalla pero las dos
  // mitades del trabajo se solapan. El cuadro terminado se copia a un bufer de
  // entrega y el hilo de presentacion lo dibuja, de modo que la simulacion no
  // espera al vertical.
  //
  // **Va apagado por defecto, y es una decisión medida, no un olvido.** En Super
  // Metroid el cuadro costaba 14 ms de los 16,7 y el hilo subió la simulación de
  // 52,9 a 58,3 fps. Aquí no: medido en la consola el 2026-09-11, el cuadro de
  // esta familia cuesta 1-2 ms (1,4 en DKC1), o sea que sobran 15 ms y no hay nada
  // que solapar. Los dos caminos dan 59,9 fps, y el del hilo tiene un coste que el
  // otro no tiene: el juego va un cuadro por delante del presentador, así que el
  // mando responde **16,7 ms más tarde**. Con margen de sobra, gana lo síncrono.
  //
  // Se enciende sin recompilar: `PresentThread=1` en `\LocalState\dkc3-host.ini`.
  bool con_hilo_ = false;
  std::thread hilo_;
  std::mutex mtx_;
  std::condition_variable cv_lleno_, cv_libre_;
  std::string entrega_;  // cuadro terminado esperando turno
  std::string subida_;   // el que el presentador está subiendo
  bool hay_cuadro_ = false;
  bool parar_ = false;
  std::atomic<unsigned long long> presentados_{0}, soltados_{0};
  std::atomic<long long> p_dib_ticks_{0}, p_pres_ticks_{0};

  // Contador de FPS en pantalla. En la consola arranca ENCENDIDO —no hay teclado ni variables
  // de entorno para pedirlo— y R3 solo sirve para quitarlo de en medio.
  bool fps_visible_ = true;
  double fps_actual_ = 0.0;
  bool r3_pulsado_ = false;

  void PintaContadorFps() {
    if (!fps_visible_ || frame_.empty()) return;
    char linea[32];
    snprintf(linea, sizeof linea, "%.1f FPS %.2fx", fps_actual_, fps_actual_ / 60.0);
    FpsPintaTexto((unsigned char *)&frame_[0], fw_, kVideoHeight, 4, 4, linea);
  }

  Etapas acc_;
  unsigned long long cuadros_ = 0;
  long long t_latido_ = 0;
  unsigned long long p_previos_ = 0;
  long long dib_previos_ = 0, pres_previos_ = 0;

  // --- IFrameworkViewSource ---
  wac::IFrameworkView CreateView() { return *this; }

  // --- IFrameworkView ---
  void Initialize(wac::CoreApplicationView const &vista) {
    // Aquí es donde la consola dice si viene de terminarnos. Ojo: en la Xbox
    // `AppInstance::GetActivatedEventArgs()` devuelve nulo, así que el dato bueno
    // es este evento y no aquella llamada.
    vista.Activated([this](wac::CoreApplicationView const &, waa::IActivatedEventArgs const &a) {
      prev_exec_.store((int)a.PreviousExecutionState());
    });
  }
  void Load(winrt::hstring const &) {}
  void Uninitialize() {}

  void SetWindow(wuc::CoreWindow const &w) {
    window_ = w;
    std::string datos;
    try {
      datos = winrt::to_string(wst::ApplicationData::Current().LocalFolder().Path());
      g_log = fopen((datos + "\\dkc3_boot.log").c_str(), "w");
    } catch (...) {}
    Log("SetWindow: entry");
    LARGE_INTEGER frec;
    QueryPerformanceFrequency(&frec);
    g_ms_por_tick = 1000.0 / (double)frec.QuadPart;
    if (!datos.empty()) LeeAjustes(datos);
    w.Closed([this](auto &&, auto &&) { Log("CoreWindow.Closed: el sistema cerro la ventana"); running_ = false; });
    w.VisibilityChanged([this](auto &&, wuc::VisibilityChangedEventArgs const &a) {
      visible_ = a.Visible();
    });

    // En la Xbox, B es el boton «atras» del sistema: cada pulsacion levanta
    // SystemNavigationManager.BackRequested y, si nadie lo marca como atendido, el
    // shell saca la app y devuelve al panel de inicio (eso cerraba el juego al
    // mantener B o pulsarlo varias veces). Aqui B es un boton del juego (SNES A),
    // asi que el evento se atiende y no se propaga. Para salir esta el boton Xbox.
    try {
      nav_ = wuc::SystemNavigationManager::GetForCurrentView();
      nav_token_ = nav_.BackRequested(
          [this](wf::IInspectable const &, wuc::BackRequestedEventArgs const &a) {
            a.Handled(true);
            Log("BackRequested: atendido (B no cierra el juego)");
          });
      Log("SetWindow: BackRequested enganchado");
    } catch (...) {
      Log("SetWindow: SystemNavigationManager no disponible");
    }

    // Red de seguridad: el mismo B llega ademas como tecla virtual (GamepadB/GoBack);
    // sin atenderla el shell tambien la lee como navegacion hacia atras.
    w.KeyDown([](auto &&, wuc::KeyEventArgs const &a) {
      auto k = a.VirtualKey();
      if (k == wsy::VirtualKey::GamepadB || k == wsy::VirtualKey::GoBack ||
          k == wsy::VirtualKey::Escape)
        a.Handled(true);
    });
    // La partida se escribe cuando el sistema avisa de que va a suspender la app:
    // en la consola eso es lo ultimo que llega con vida (boton Xbox, cambiar de
    // juego, apagar). Se pide un aplazamiento para que no corten a media escritura.
    try {
      suspend_token_ = wac::CoreApplication::Suspending(
          [this](wf::IInspectable const &, wam::SuspendingEventArgs const &a) {
            auto aplazo = a.SuspendingOperation().GetDeferral();
            Log("Suspending: el sistema va a suspender la app");
            GuardaAlSuspender();
            aplazo.Complete();
          });
      Log("SetWindow: Suspending enganchado");
    } catch (...) {
      Log("SetWindow: no se pudo enganchar Suspending");
    }

    try {
      InitD3D();
      Log("SetWindow: InitD3D ok");
    } catch (winrt::hresult_error const &e) {
      Log("SetWindow EXC hresult:");
      Log(winrt::to_string(e.message()).c_str());
    } catch (std::exception const &e) {
      Log("SetWindow EXC std:");
      Log(e.what());
    } catch (...) {
      Log("SetWindow EXC unknown");
    }
  }

  void Run() {
    Log("Run: entry");
    hilo_bucle_ = std::this_thread::get_id();
    window_.Activate();
    Log("Run: activated");
    try {
      BootGame();
    } catch (winrt::hresult_error const &e) {
      Log("Boot EXC hresult:");
      Log(winrt::to_string(e.message()).c_str());
    } catch (std::exception const &e) {
      Log("Boot EXC std:");
      Log(e.what());
    } catch (...) {
      Log("Boot EXC unknown");
    }
    Logf("Run: booted=%ld", (long)booted_);
    if (booted_ && con_hilo_) ArrancaPresentador();
    auto disp = window_.Dispatcher();
    long long t = Ahora();
    t_latido_ = t;
    while (running_) {
      disp.ProcessEvents(wuc::CoreProcessEventsOption::ProcessAllIfPresent);
      if (!running_) break;
      // Punto seguro para volcar la partida: aqui no hay ningun cuadro a medias.
      if (pide_guardar_.exchange(false)) { EscribeSram("peticion del sistema"); GuardaEstado("suspension"); }
      // Volver donde lo dejaste. Primero hay que saber de dónde venimos: el aviso
      // de activación llega por el despachador, así que se espera a tenerlo (con
      // dos segundos de tope por si no llegara). Después, a guardar cada 10 s.
      if (booted_ && !estado_resuelto_) {
        const int pe = prev_exec_.load();
        if (pe >= 0 || ++cuadros_resolver_ > 120) {
          estado_resuelto_ = true;
          if (pe == 3) RestauraEstado();
          else Logf("suspension: arranque normal (estado previo %ld)", (long)pe);
          t_estado_ = Ahora();
        }
      }
      if (booted_ && estado_resuelto_ && (Ahora() - t_estado_) * g_ms_por_tick >= 10000.0) {
        t_estado_ = Ahora();
        GuardaEstado("periodico");
      }
      if (!booted_ || !visible_) {
        disp.ProcessEvents(wuc::CoreProcessEventsOption::ProcessOneAndAllPending);
        t = Ahora();
        continue;
      }
      uint32_t input = PollGamepad();
      acc_.evt += Ms(&t);

      RtlRunFrame(input);
      acc_.emu += Ms(&t);
      if (g_fail || !Dkc3LastLleResult()) { Log("Run: el motor pidio parar (g_fail / LleResult)"); running_ = false; break; }
      Dkc3DrawPpuFrame();
      acc_.comp += Ms(&t);
      PumpAudio();
      acc_.aud += Ms(&t);
      PintaContadorFps();   /* Contador de FPS en pantalla */
      EntregaCuadro(&t);
      Latido();
    }
    if (booted_) EscribeSram("salida del bucle");
    ParaPresentador();
  }

  // --- Cadencia: entrega del cuadro y latido ---

  // Con el hilo apagado hace lo de siempre (subir, dibujar y esperar al vertical);
  // con el hilo encendido copia el cuadro al búfer de entrega (~300 KB) y espera a
  // que el presentador se lleve el anterior, que es lo que marca el ritmo.
  void EntregaCuadro(long long *t) {
    if (!con_hilo_) {
      const long long dib0 = p_dib_ticks_.load(), pre0 = p_pres_ticks_.load();
      DibujaYPresenta((const uint8_t *)frame_.data());
      acc_.ent += (p_dib_ticks_.load() - dib0) * g_ms_por_tick;
      acc_.espera += (p_pres_ticks_.load() - pre0) * g_ms_por_tick;
      *t = Ahora();
      return;
    }
    bool pisado = false;
    {
      std::unique_lock<std::mutex> cerrojo(mtx_);
      if (hay_cuadro_)
        cv_libre_.wait_for(cerrojo, std::chrono::milliseconds(50),
                           [this] { return !hay_cuadro_ || parar_; });
      acc_.espera += Ms(t);
      pisado = hay_cuadro_;  // el presentador no llegó: se pisa el anterior
      if (entrega_.size() != frame_.size()) entrega_.assign(frame_.size(), 0);
      std::memcpy(&entrega_[0], frame_.data(), frame_.size());
      hay_cuadro_ = true;
    }
    cv_lleno_.notify_one();
    acc_.ent += Ms(t);
    if (pisado) soltados_.fetch_add(1);
  }

  // Una línea por segundo al registro: los dos relojes (cuadros simulados y
  // presentaciones) y el reparto del tiempo. `trabajo` es lo que cuesta el cuadro
  // sin contar esperas; si se acerca a 16,7 ms no hay margen y se pierden cuadros.
  void Latido() {
    if ((++cuadros_ % 60) != 0) return;
    long long ahora = Ahora();
    const double ventana = (ahora - t_latido_) * g_ms_por_tick;
    t_latido_ = ahora;
    if (ventana <= 0) return;
    fps_actual_ = 60000.0 / ventana;   /* Contador de FPS en pantalla */

    const unsigned long long p = presentados_.load();
    const unsigned long long dp = p - p_previos_;
    p_previos_ = p;
    const long long dib = p_dib_ticks_.load(), pre = p_pres_ticks_.load();
    const double dib_ms = (dib - dib_previos_) * g_ms_por_tick;
    const double pres_ms = (pre - pres_previos_) * g_ms_por_tick;
    dib_previos_ = dib;
    pres_previos_ = pre;

    const double n = 60.0;
    const double trabajo = (acc_.emu + acc_.comp + acc_.aud + acc_.ent + acc_.evt) / n;
    char m[512];
    snprintf(m, sizeof m,
             "latido f=%llu sim=%.1f fps pres=%.1f Hz | emu=%.2f comp=%.2f aud=%.2f "
             "ent=%.2f evt=%.2f espera=%.2f | dibujar=%.2f presentar=%.2f | trabajo=%.2f ms | soltados=%llu",
             cuadros_, 60000.0 / ventana, dp * 1000.0 / ventana, acc_.emu / n, acc_.comp / n,
             acc_.aud / n, acc_.ent / n, acc_.evt / n, acc_.espera / n,
             dp ? dib_ms / (double)dp : 0.0, dp ? pres_ms / (double)dp : 0.0, trabajo,
             soltados_.load());
    Log(m);
    acc_ = Etapas{};
  }

  // --- El hilo de presentación ---
  void ArrancaPresentador() {
    parar_ = false;
    hilo_ = std::thread([this] { HiloDePresentacion(); });
  }

  void ParaPresentador() {
    if (hilo_.joinable()) {
      {
        std::lock_guard<std::mutex> cerrojo(mtx_);
        parar_ = true;
        hay_cuadro_ = false;
      }
      cv_lleno_.notify_all();
      cv_libre_.notify_all();
      hilo_.join();
    }
    char m[160];
    snprintf(m, sizeof m, "presentador: total %llu cuadros, %llu presentados, %llu soltados",
             cuadros_, presentados_.load(), soltados_.load());
    Log(m);
  }

  void HiloDePresentacion() {
    Log("presentador: hilo arrancado");
    for (;;) {
      {
        std::unique_lock<std::mutex> cerrojo(mtx_);
        cv_lleno_.wait(cerrojo, [this] { return hay_cuadro_ || parar_; });
        if (!hay_cuadro_) break;  // solo queda la orden de parar
        subida_.swap(entrega_);   // intercambio de punteros, no copia
        hay_cuadro_ = false;
      }
      cv_libre_.notify_one();
      DibujaYPresenta((const uint8_t *)subida_.data());
    }
    Log("presentador: hilo terminado");
  }

  // `\LocalState\dkc3-host.ini` (opcional): `PresentThread=1` enciende el hilo de
  // presentación. Se sube con el Device Portal, así que el A/B se hace con el
  // MISMO binario y sin volver a instalar nada.
  void LeeAjustes(const std::string &carpeta) {
    FILE *f = fopen((carpeta + "\\dkc3-host.ini").c_str(), "r");
    if (f) {
      char linea[256], clave[64], valor[64];
      while (fgets(linea, sizeof linea, f)) {
        if (sscanf(linea, " %63[^= \t] = %63s", clave, valor) != 2) continue;
        if (std::strcmp(clave, "PresentThread") == 0) con_hilo_ = valor[0] != '0';
      }
      fclose(f);
    }
    Log(con_hilo_ ? "ajustes: hilo de presentacion = si" : "ajustes: hilo de presentacion = no");
  }

  // --- D3D ---
  void InitD3D() {
    Log("InitD3D: entry");
    auto b = window_.Bounds();
    bbw_ = (UINT)(b.Width > 0 ? b.Width : 1920);
    bbh_ = (UINT)(b.Height > 0 ? b.Height : 1080);

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL fl[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    HRESULT hrd = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, fl,
                      _countof(fl), D3D11_SDK_VERSION, device_.GetAddressOf(),
                      nullptr, ctx_.GetAddressOf());
    Logf("InitD3D: device hr=0x%lx", (long)hrd);

    ComPtr<IDXGIDevice1> dxgiDev;
    device_.As(&dxgiDev);
    ComPtr<IDXGIAdapter> adapter;
    dxgiDev->GetAdapter(adapter.GetAddressOf());
    ComPtr<IDXGIFactory2> factory;
    adapter->GetParent(__uuidof(IDXGIFactory2), &factory);

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = bbw_;
    sd.Height = bbh_;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    sd.Scaling = DXGI_SCALING_STRETCH;
    IUnknown *coreWin = winrt::get_unknown(window_);
    HRESULT hrsc = factory->CreateSwapChainForCoreWindow(device_.Get(), coreWin, &sd, nullptr,
                                          swap_.GetAddressOf());
    Logf("InitD3D: swapchain hr=0x%lx", (long)hrsc);
    MakeRTV();

    // Textura del cuadro (dinámica, BGRA, 342x224).
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = 342;
    td.Height = kVideoHeight;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DYNAMIC;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device_->CreateTexture2D(&td, nullptr, tex_.GetAddressOf());
    device_->CreateShaderResourceView(tex_.Get(), nullptr, srv_.GetAddressOf());

    D3D11_SAMPLER_DESC smp = {};
    smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device_->CreateSamplerState(&smp, samp_.GetAddressOf());

    device_->CreateVertexShader(g_VS, g_VS_len, nullptr, vs_.GetAddressOf());
    device_->CreatePixelShader(g_PS, g_PS_len, nullptr, ps_.GetAddressOf());

    InitAudio();
  }

  void MakeRTV() {
    ComPtr<ID3D11Texture2D> bb;
    swap_->GetBuffer(0, __uuidof(ID3D11Texture2D), &bb);
    device_->CreateRenderTargetView(bb.Get(), nullptr, rtv_.ReleaseAndGetAddressOf());
  }

  // Sube el cuadro a la textura, lo dibuja y **espera al vertical**. Es el único
  // sitio que usa el contexto de Direct3D 11, y lo ejecuta un solo hilo: el de
  // presentación, o el del juego si el hilo está apagado.
  void DibujaYPresenta(const uint8_t *pixeles) {
    if (!pixeles || !swap_ || !tex_) return;
    long long t = Ahora();

    // Subir el cuadro CPU -> textura.
    D3D11_MAPPED_SUBRESOURCE m;
    if (SUCCEEDED(ctx_->Map(tex_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
      const int rowbytes = fw_ * 4;
      const uint8_t *src = pixeles;
      uint8_t *dst = (uint8_t *)m.pData;
      for (int y = 0; y < kVideoHeight; ++y)
        std::memcpy(dst + (size_t)y * m.RowPitch, src + (size_t)y * rowbytes, rowbytes);
      ctx_->Unmap(tex_.Get(), 0);
    }

    D3D11_VIEWPORT vp = {0, 0, (float)bbw_, (float)bbh_, 0, 1};
    ctx_->RSSetViewports(1, &vp);
    ID3D11RenderTargetView *rtv = rtv_.Get();
    ctx_->OMSetRenderTargets(1, &rtv, nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->IASetInputLayout(nullptr);
    ctx_->VSSetShader(vs_.Get(), nullptr, 0);
    ctx_->PSSetShader(ps_.Get(), nullptr, 0);
    ID3D11ShaderResourceView *srv = srv_.Get();
    ctx_->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState *smp = samp_.Get();
    ctx_->PSSetSamplers(0, 1, &smp);
    ctx_->Draw(3, 0);
    p_dib_ticks_.fetch_add(Ticks(&t));

    swap_->Present(1, 0);
    p_pres_ticks_.fetch_add(Ticks(&t));
    presentados_.fetch_add(1);
  }

  // --- Audio ---
  void InitAudio() {
    if (FAILED(XAudio2Create(&xaudio_, 0, XAUDIO2_DEFAULT_PROCESSOR))) return;
    if (FAILED(xaudio_->CreateMasteringVoice(&master_, kAudioChannels, kAudioRate))) return;
    WAVEFORMATEX wf = {};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = kAudioChannels;
    wf.nSamplesPerSec = kAudioRate;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    if (FAILED(xaudio_->CreateSourceVoice(&voice_, &wf, 0, 2.0f, &audio_cb_))) {
      voice_ = nullptr;
      return;
    }
    RtlSetAudioOutputRate(kAudioRate);
    voice_->Start(0);
  }

  void PumpAudio() {
    if (!voice_) return;
    XAUDIO2_VOICE_STATE st;
    voice_->GetState(&st);
    if (st.BuffersQueued >= 6) return;  // no adelantar demasiado
    const int frames = kAudioRate / 60;  // ~800 muestras estéreo por cuadro
    int16_t *block = new int16_t[(size_t)frames * kAudioChannels];
    RtlRenderAudio(block, frames, kAudioChannels);
    XAUDIO2_BUFFER xb = {};
    xb.AudioBytes = (UINT32)(frames * kAudioChannels * sizeof(int16_t));
    xb.pAudioData = (const BYTE *)block;
    xb.pContext = block;
    if (FAILED(voice_->SubmitSourceBuffer(&xb))) delete[] block;
  }

  // --- Entrada ---
  uint32_t PollGamepad() {
    auto pads = wgi::Gamepad::Gamepads();
    if (pads.Size() == 0) return 0;
    wgi::GamepadReading r = pads.GetAt(0).GetCurrentReading();
    auto down = [&](wgi::GamepadButtons b) {
      return (r.Buttons & b) != wgi::GamepadButtons::None;
    };
    uint32_t in = 0;
    // Contador de FPS en pantalla: R3 lo enciende y lo apaga. El mando de SNES no tiene clics de
    // stick, así que este botón no le quita nada al juego.
    const bool r3 = down(wgi::GamepadButtons::RightThumbstick);
    if (r3 && !r3_pulsado_) {
      fps_visible_ = !fps_visible_;
      Log(fps_visible_ ? "FPS readout on" : "FPS readout off");
    }
    r3_pulsado_ = r3;

    if (down(wgi::GamepadButtons::A)) in |= kB;             // saltar
    if (down(wgi::GamepadButtons::X)) in |= kY;             // correr/agarrar
    if (down(wgi::GamepadButtons::B)) in |= kA;
    if (down(wgi::GamepadButtons::Y)) in |= kX;
    if (down(wgi::GamepadButtons::LeftShoulder)) in |= kL;
    if (down(wgi::GamepadButtons::RightShoulder)) in |= kR;
    if (down(wgi::GamepadButtons::Menu)) in |= kStart;
    if (down(wgi::GamepadButtons::View)) in |= kSelect;
    if (down(wgi::GamepadButtons::DPadUp) || r.LeftThumbstickY > 0.5) in |= kUp;
    if (down(wgi::GamepadButtons::DPadDown) || r.LeftThumbstickY < -0.5) in |= kDown;
    if (down(wgi::GamepadButtons::DPadLeft) || r.LeftThumbstickX < -0.5) in |= kLeft;
    if (down(wgi::GamepadButtons::DPadRight) || r.LeftThumbstickX > 0.5) in |= kRight;
    return in;
  }

  // --- Arranque del juego ---
  std::string Utf8(winrt::hstring const &h) { return winrt::to_string(h); }

  void BootGame() {
    // Guardar SRAM/estados en LocalState (el install dir es de solo lectura).
    std::string saves = Utf8(wst::ApplicationData::Current().LocalFolder().Path()) + "\\saves";
    RtlSetSaveRoot(saves.c_str());
    CreateDirectoryA(saves.c_str(), nullptr);   // el motor no la crea; el estado se escribe aquí
    estado_ruta_ = saves + "\\suspension.state";

    std::string rom_path = Utf8(wam::Package::Current().InstalledLocation().Path()) + "\\dkc3.sfc";
    size_t rom_size = 0;
    char err[192] = {0};
    Log("BootGame: rom path:");
    Log(rom_path.c_str());
    uint8_t *rom = Dkc3ReadVerifiedRom(rom_path.c_str(), &rom_size, err, sizeof err);
    if (!rom) { Log("BootGame: ROM read FAILED:"); Log(err); running_ = false; return; }
    Logf("BootGame: ROM ok bytes=%ld", (long)rom_size);

    Dkc3VideoSetWidescreen(true);
    Dkc3VideoSetAspect(kDkc3VideoAspect16x9);
    RtlRegisterGame(Dkc3GameInfo());
    if (!SnesInit(rom, (int)rom_size)) { Log("BootGame: SnesInit FAILED"); running_ = false; return; }
    Log("BootGame: SnesInit ok");

    // La pila del cartucho vive en `<raiz>\save.srm`. Se carga DESPUES de
    // SnesInit, que es quien apunta g_sram a la RAM del cartucho recien creado.
    RtlReadSram();
    Log("BootGame: raiz de guardado:");
    Log(RtlSaveRoot());

    fw_ = Dkc3VideoWidth();
    frame_.assign((size_t)fw_ * kVideoHeight * 4, 0);
    Dkc3BeginDrawing((uint8_t *)frame_.data(), (size_t)fw_ * 4);
    booted_ = true;
    Log("BootGame: booted");
  }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  wac::CoreApplication::Run(winrt::make<App>());
  return 0;
}
