#include <clap/clap.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
constexpr clap_id PARAM_CEILING = 0;
constexpr clap_id PARAM_RELEASE = 1;
constexpr clap_id PARAM_LOOKAHEAD = 2;
constexpr clap_id PARAM_INPUT = 3;

struct Limiter {
  const clap_host *host = nullptr;
  clap_plugin plugin{};
  double sample_rate = 44100.0;
  float ceiling_db = -1.0f;
  float release_ms = 80.0f;
  float lookahead_ms = 5.0f;
  float input_db = 0.0f;
  float gain = 1.0f;
  uint32_t delay_len = 1;
  uint32_t delay_pos = 0;
  std::vector<float> delay_l, delay_r;
  float last_reduction_db = 0.0f;

  float ceiling() const { return std::pow(10.0f, ceiling_db / 20.0f); }
  void prepare(double sr) {
    sample_rate = std::max(8000.0, sr);
    delay_len = std::max<uint32_t>(1, (uint32_t)std::ceil(sample_rate * lookahead_ms * 0.001));
    delay_l.assign(delay_len, 0.0f);
    delay_r.assign(delay_len, 0.0f);
    delay_pos = 0;
    gain = 1.0f;
  }
};

static Limiter *self(const clap_plugin *p) { return static_cast<Limiter *>(p->plugin_data); }
static bool CLAP_ABI init(const clap_plugin *p) { return p != nullptr; }
static void CLAP_ABI destroy(const clap_plugin *p) { delete self(p); }
static bool CLAP_ABI activate(const clap_plugin *p, double sr, uint32_t, uint32_t) {
  self(p)->prepare(sr); return true;
}
static void CLAP_ABI deactivate(const clap_plugin *) {}
static bool CLAP_ABI start_processing(const clap_plugin *) { return true; }
static void CLAP_ABI stop_processing(const clap_plugin *) {}
static void CLAP_ABI reset(const clap_plugin *p) { self(p)->prepare(self(p)->sample_rate); }
static void CLAP_ABI on_main_thread(const clap_plugin *) {}

static uint32_t CLAP_ABI audio_ports_count(const clap_plugin *, bool) { return 1; }
static bool CLAP_ABI audio_ports_get(const clap_plugin *, uint32_t index, bool is_input, clap_audio_port_info *info) {
  if (index != 0) return false;
  info->id = is_input ? 10 : 20;
  info->name[0] = '\0';
  const char *name = is_input ? "Stereo Input" : "Stereo Output";
  std::snprintf(info->name, sizeof(info->name), "%s", name);
  info->flags = CLAP_AUDIO_PORT_IS_MAIN;
  info->channel_count = 2;
  info->port_type = CLAP_PORT_STEREO;
  info->in_place_pair = is_input ? 20 : 10;
  return true;
}

static const clap_plugin_audio_ports audio_ports_ext;\n\nstatic void set_param(Limiter *s, clap_id id, double value) {
  switch (id) {
    case PARAM_CEILING: s->ceiling_db = (float)std::clamp(value, -12.0, 0.0); break;
    case PARAM_RELEASE: s->release_ms = (float)std::clamp(value, 5.0, 500.0); break;
    case PARAM_LOOKAHEAD: s->lookahead_ms = (float)std::clamp(value, 0.0, 10.0); break;
    case PARAM_INPUT: s->input_db = (float)std::clamp(value, -24.0, 24.0); break;
  }
}
static uint32_t CLAP_ABI params_count(const clap_plugin *) { return 4; }
static bool CLAP_ABI params_info(const clap_plugin *, uint32_t index, clap_param_info *info) {
  static const char *names[] = {"Ceiling", "Release", "Lookahead", "Input Gain"};
  static const char *units[] = {" dBFS", " ms", " ms", " dB"};
  static const double mins[] = {-12, 5, 0, -24};
  static const double maxs[] = {0, 500, 10, 24};
  static const double defs[] = {-1, 80, 5, 0};
  if (index >= 4) return false;
  info->id = index; info->flags = CLAP_PARAM_IS_AUTOMATABLE;
  info->cookie = nullptr;
  std::snprintf(info->name, sizeof(info->name), "%s", names[index]);
  std::snprintf(info->module, sizeof(info->module), "Limiter");
  info->min_value = mins[index]; info->max_value = maxs[index]; info->default_value = defs[index];
  (void)units;
  return true;
}
static bool CLAP_ABI params_value(const clap_plugin *p, clap_id id, double *v) {
  auto *s = self(p);
  switch (id) {
    case PARAM_CEILING: *v = s->ceiling_db; return true;
    case PARAM_RELEASE: *v = s->release_ms; return true;
    case PARAM_LOOKAHEAD: *v = s->lookahead_ms; return true;
    case PARAM_INPUT: *v = s->input_db; return true;
    default: return false;
  }
}
static bool CLAP_ABI params_to_text(const clap_plugin *, clap_id id, double v, char *buf, uint32_t size) {
  const char *unit = id == PARAM_CEILING || id == PARAM_INPUT ? " dB" : " ms";
  std::snprintf(buf, size, "%.2f%s", v, unit); return true;
}
static bool CLAP_ABI params_from_text(const clap_plugin *, clap_id id, const char *txt, double *v) {
  if (!txt || !v || id > PARAM_INPUT) return false;
  char *end = nullptr; double x = std::strtod(txt, &end);
  if (end == txt) return false; *v = x; return true;
}
static const clap_plugin_params params_ext = {params_count, params_info, params_value, params_to_text, params_from_text};
static const void *CLAP_ABI get_extension(const clap_plugin *, const char *id) {
  if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &audio_ports_ext;
  if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &params_ext;
  return nullptr;
}

static clap_process_status CLAP_ABI process(const clap_plugin *p, const clap_process *proc) {
  auto *s = self(p);
  if (!proc || proc->audio_inputs_count < 1 || proc->audio_outputs_count < 1) return CLAP_PROCESS_ERROR;
  auto *in = proc->audio_inputs[0]; auto *out = proc->audio_outputs[0];
  if (in->channel_count < 2 || out->channel_count < 2 || !in->data32 || !out->data32) return CLAP_PROCESS_ERROR;

  // Apply parameter changes at their sample offsets.
  uint32_t ev = 0;
  const float input_gain = std::pow(10.0f, s->input_db / 20.0f);
  const float limit = s->ceiling();
  const float release_coeff = std::exp(-1.0f / (float)(s->sample_rate * s->release_ms * 0.001));
  const float attack_coeff = 0.0f; // instantaneous peak control; lookahead supplies the delay
  for (uint32_t i = 0; i < proc->frames_count; ++i) {
    while (proc->in_events && ev < proc->in_events->size()) {
      const clap_event_header *h = proc->in_events->get(proc->in_events, ev);
      if (!h || h->time > i) break;
      if (h->space_id == CLAP_CORE_EVENT_SPACE_ID && h->type == CLAP_EVENT_PARAM_VALUE) {
        const auto *e = (const clap_event_param_value *)h;
        set_param(s, e->param_id, e->value);
      }
      ++ev;
    }
    const float l = in->data32[0][i] * input_gain;
    const float r = in->data32[1][i] * input_gain;
    const float peak = std::max(std::abs(l), std::abs(r));
    const float target = peak > limit ? limit / peak : 1.0f;
    if (target < s->gain) s->gain = target;
    else s->gain = release_coeff * s->gain + (1.0f - release_coeff);
    (void)attack_coeff;

    const float delayed_l = s->delay_l[s->delay_pos];
    const float delayed_r = s->delay_r[s->delay_pos];
    s->delay_l[s->delay_pos] = l;
    s->delay_r[s->delay_pos] = r;
    s->delay_pos = (s->delay_pos + 1) % s->delay_len;
    out->data32[0][i] = delayed_l * s->gain;
    out->data32[1][i] = delayed_r * s->gain;
  }
  out->constant_mask = 0;
  return CLAP_PROCESS_CONTINUE;
}

static const clap_plugin_descriptor descriptor = {
  CLAP_VERSION, "audio.rrrastas.advancedlimiterfx", "Advanced Limiter FX",
  "RRRastas", "https://github.com/free-audio/clap", nullptr, nullptr,
  "1.0.0", "Transparent lookahead peak limiter inspired by foobar2000 Advanced Limiter DSP",
  nullptr
};
static const clap_plugin_audio_ports audio_ports_ext = {audio_ports_count, audio_ports_get};

static const clap_plugin *CLAP_ABI create_plugin(const clap_plugin_factory *, const clap_host *host, const char *id) {
  if (std::strcmp(id, descriptor.id)) return nullptr;
  auto *s = new Limiter(); s->host = host;
  s->plugin = {&descriptor, s, init, destroy, activate, deactivate, start_processing, stop_processing, reset, process, get_extension, on_main_thread};
  return &s->plugin;
}
static uint32_t CLAP_ABI factory_count(const clap_plugin_factory *) { return 1; }
static const clap_plugin_descriptor *CLAP_ABI factory_desc(const clap_plugin_factory *, uint32_t i) { return i == 0 ? &descriptor : nullptr; }
static const clap_plugin_factory factory = {factory_count, factory_desc, create_plugin};
static bool CLAP_ABI entry_init(const char *) { return true; }
static void CLAP_ABI entry_deinit() {}
static const void *CLAP_ABI entry_get_factory(const char *id) {
  return std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) == 0 ? &factory : nullptr;
}
}

extern "C" CLAP_EXPORT const clap_plugin_entry clap_entry = {
  CLAP_VERSION, entry_init, entry_deinit, entry_get_factory
};
