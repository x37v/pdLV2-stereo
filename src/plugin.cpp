/*
 * based on work by:
 * Martin Schied https://github.com/unknownError/pdLV2-stereo
 * Lars Luthman http://www.nongnu.org/ll-plugins/lv2pftci/
 */

#include <lv2plugin.hpp>
#include <iostream>
#include <libpd/z_libpd.h>
#include <atomic>
#include <functional>
#include <fstream>
#include <cstdio>

#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>

#include "plugin.h"
#include "uridmixin.hpp"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>

#define CHAN_MASK 0x07

using namespace LV2;
using std::cout;
using std::cerr;
using std::endl;
namespace sp = std::placeholders;

class PDLv2Plugin;
namespace {

  void copy_file(std::string from, std::string to) {
    std::ifstream source(from, std::ios::binary);
    std::ofstream dest(to, std::ios::binary);

    dest << source.rdbuf();

    source.close();
    dest.close();
  }


  template<typename R, typename ...Args>
    R call_pd(void * library_handle, std::string func_name, Args ...args) {
      R (*ptr)(Args...);
      ptr = (R (*)(Args...))dlsym(library_handle, func_name.c_str());
      if (!ptr) {
        cerr << "couldn't get function " << func_name << endl;
        return R();
      }
      return (*ptr)(std::forward<Args>(args)...);
    }

  template<typename R>
    R call_pd_void(void * library_handle, std::string func_name) {
      R (*ptr)(void);
      ptr = (R (*)(void))dlsym(library_handle, func_name.c_str());
      if (!ptr) {
        cerr << "couldn't get function " << func_name << endl;
        return R();
      }
      return (*ptr)();
    }

  template<typename ...Args>
    void call_pd_ret_void(void * library_handle, std::string func_name, Args ...args) {
      void (*ptr)(Args...);
      ptr = (void (*)(Args...))dlsym(library_handle, func_name.c_str());
      if (!ptr) {
        cerr << "couldn't get function " << func_name << endl;
        return;
      }
      (*ptr)(std::forward<Args>(args)...);
    }

  PDLv2Plugin * current_plugin = nullptr;

  std::atomic_flag pd_global_lock = ATOMIC_FLAG_INIT;

  void pdprint(const char *s) {
    cout << s;
  }

  void pd_floathook(const char *s, float value);
  void pd_noteonhook(int channel, int pitch, int velocity);
  void pd_controlchangehook(int channel, int controller, int value);
  void pd_programchangehook(int channel, int value);
  void pd_pitchbendhook(int channel, int value);
  void pd_aftertouchhook(int channel, int value);
  void pd_polyaftertouchhook(int channel, int pitch, int value);

  void with_lock(std::function<void()> func) {
    while (pd_global_lock.test_and_set(std::memory_order_acquire));  // spin, acquire lock
    func();
    pd_global_lock.clear(std::memory_order_release); // release lock
  }
}

class PDLv2Plugin :
  public Plugin<PDLv2Plugin, LV2::URID<true>>
{
  public:
    PDLv2Plugin(double rate) : Plugin<PDLv2Plugin, LV2::URID<true>>(pdlv2::ports.size()) {
      const std::string plugin_bundle_path(bundle_path());
      std::string so_path = plugin_bundle_path + "/libpd.so";
      mLIBPDUniquePath = std::string(std::tmpnam(nullptr)) + "-libpd.so";

      copy_file(so_path, mLIBPDUniquePath);
      //XXX what flags do we need now since we have our own unique plugin?
      mLIBPDHandle = dlopen(mLIBPDUniquePath.c_str(), RTLD_NOW | RTLD_DEEPBIND | RTLD_LOCAL);
      if (mLIBPDHandle == NULL) {
        cerr << "cannot load libpd library" << endl;
        set_ok(false);
        return;
      }

      for (size_t i = 0; i < pdlv2::ports.size(); i++) {
        pdlv2::PortInfo info = pdlv2::ports.at(i);
        switch (info.type) {
          case pdlv2::AUDIO_IN:
            mAudioIn.push_back(i);
            break;
          case pdlv2::AUDIO_OUT:
            mAudioOut.push_back(i);
            break;
          case pdlv2::CONTROL_IN:
          case pdlv2::CONTROL_OUT:
            break;
        }
      }

      call_pd_ret_void<t_libpd_printhook>(mLIBPDHandle, "libpd_set_printhook", (t_libpd_printhook)pdprint);

      if (call_pd<int, const char *>(mLIBPDHandle, "libpd_exists", "PDLV2-TEST") != 0) {
        cout << plugin_bundle_path << " EXISTS" << endl;
      } else {
        call_pd_void<int>(mLIBPDHandle, "libpd_init");
        call_pd<void*, const char *>(mLIBPDHandle, "libpd_bind", "PDLV2-TEST");
        call_pd<int, int, int, int>(mLIBPDHandle, "libpd_init_audio", mAudioIn.size(), mAudioOut.size(), static_cast<int>(rate)); 
      }

      call_pd_ret_void<const t_libpd_floathook>(mLIBPDHandle, "libpd_set_floathook", &pd_floathook);
      call_pd_ret_void<const t_libpd_noteonhook>(mLIBPDHandle, "libpd_set_noteonhook", &pd_noteonhook);
      call_pd_ret_void<const t_libpd_controlchangehook>(mLIBPDHandle, "libpd_set_controlchangehook", &pd_controlchangehook);
      call_pd_ret_void<const t_libpd_programchangehook>(mLIBPDHandle, "libpd_set_programchangehook", &pd_programchangehook);
      call_pd_ret_void<const t_libpd_pitchbendhook>(mLIBPDHandle, "libpd_set_pitchbendhook", &pd_pitchbendhook);
      call_pd_ret_void<const t_libpd_aftertouchhook>(mLIBPDHandle, "libpd_set_aftertouchhook", &pd_aftertouchhook);
      call_pd_ret_void<const t_libpd_polyaftertouchhook>(mLIBPDHandle, "libpd_set_polyaftertouchhook", &pd_polyaftertouchhook);

      mPatchFileHandle = call_pd<void *, const char *, const char *>(mLIBPDHandle, "libpd_openfile", pdlv2::patch_file_name, plugin_bundle_path.c_str());
      mPDDollarZero = call_pd<int, void *>(mLIBPDHandle, "libpd_getdollarzero", mPatchFileHandle); // get dollarzero from patch
      mPDBlockSize = call_pd_void<int>(mLIBPDHandle, "libpd_blocksize");

      for (size_t i = 0; i < pdlv2::ports.size(); i++) {
        pdlv2::PortInfo info = pdlv2::ports.at(i);
        switch (info.type) {
          case pdlv2::AUDIO_IN:
          case pdlv2::AUDIO_OUT:
            break;
          case pdlv2::CONTROL_IN:
            mControlIn[i] = std::to_string(mPDDollarZero) + "-lv2-" + info.name;
            break;
          case pdlv2::CONTROL_OUT:
            mControlOut[i] = std::to_string(mPDDollarZero) + "-lv2-" + info.name;
            call_pd<void*, const char *>(mLIBPDHandle, "libpd_bind", mControlOut[i].c_str());
            break;
          case pdlv2::MIDI_OUT:
            mMIDIOut[i] = midi_out_data_t();
            lv2_atom_forge_init(&mMIDIOut[i].forge, urid_map_handle());
            break;
          case pdlv2::MIDI_IN:
            mMIDIIn[i] = info.name;
            break;
        }
      }

      mPDInputBuffer.resize(mPDBlockSize * mAudioIn.size());
      mPDOutputBuffer.resize(mPDBlockSize * mAudioOut.size());

    }

    virtual ~PDLv2Plugin() {
      call_pd_ret_void<void *>(mLIBPDHandle, "libpd_closefile", mPatchFileHandle);
      dlclose(mLIBPDHandle);
      std::remove(mLIBPDUniquePath.c_str());
    }

    template<std::size_t SIZE>
    void handle_midi_out(std::array<uint8_t, SIZE> midi_bytes) {
      for (auto& kv: mMIDIOut) {
        LV2_Atom_Sequence* midibuf = p<LV2_Atom_Sequence>(kv.first);
        uint32_t capacity = midibuf->atom.size;

        lv2_atom_forge_frame_time(&kv.second.forge, mFrameTime);
        lv2_atom_forge_atom(&kv.second.forge, SIZE, mIds.midi_event);
        lv2_atom_forge_raw(&kv.second.forge, &midi_bytes.front(), SIZE);
        lv2_atom_forge_pad(&kv.second.forge, SIZE);
      }
    }

    void process_float(std::string prefix, float value) {
      for (auto& kv: mControlOut) {
        if (prefix == kv.second) {
          *p(kv.first) = value;
          break;
        }
      }
    }

    void process_noteon(int channel, int pitch, int velocity) {
      std::array<uint8_t, 3> params = {
        LV2_MIDI_MSG_NOTE_ON | static_cast<uint8_t>(channel) & 0x07,
        static_cast<uint8_t>(pitch) & 0x7F,
        static_cast<uint8_t>(velocity) & 0x7F
      };
      handle_midi_out(params);
    }
    void process_cc(int channel, int controller, int value) {
      std::array<uint8_t, 3> params = {
        LV2_MIDI_MSG_CONTROLLER | static_cast<uint8_t>(channel) & 0x07,
        static_cast<uint8_t>(controller) & 0x7F,
        static_cast<uint8_t>(value) & 0x7F
      };
      handle_midi_out(params);
    }
    void process_pgrmchg(int channel, int value) {
      std::array<uint8_t, 2> params = {
        LV2_MIDI_MSG_PGM_CHANGE | static_cast<uint8_t>(channel) & 0x07,
        static_cast<uint8_t>(value) & 0x7F
      };
      handle_midi_out(params);
    }
    void process_bend(int channel, int value) {
      std::array<uint8_t, 3> params = {
        LV2_MIDI_MSG_CONTROLLER | static_cast<uint8_t>(channel) & 0x07,
        static_cast<uint8_t>(value) & 0x7F,
        static_cast<uint8_t>(value >> 7) & 0x7F
      };
      handle_midi_out(params);
    }
    void process_touch(int channel, int value) {
      std::array<uint8_t, 2> params = {
        LV2_MIDI_MSG_CHANNEL_PRESSURE | static_cast<uint8_t>(channel) & 0x07,
        static_cast<uint8_t>(value) & 0x7F
      };
      handle_midi_out(params);
    }
    void process_poly_touch(int channel, int pitch, int value) {
      std::array<uint8_t, 3> params = {
        LV2_MIDI_MSG_NOTE_PRESSURE | static_cast<uint8_t>(channel) & 0x07,
        static_cast<uint8_t>(pitch) & 0x7F,
        static_cast<uint8_t>(value) & 0x7F
      };
      handle_midi_out(params);
    }

    void activate() {
      mIds.midi_event = map_uri(LV2_MIDI__MidiEvent);

      call_pd<int, int>(mLIBPDHandle, "libpd_start_message", 1);  // begin of message
      call_pd_ret_void<float>(mLIBPDHandle, "libpd_add_float", 1.0f);  // message contains now "1"
      call_pd<int, const char *, const char *>(mLIBPDHandle, "libpd_finish_message", "pd", "dsp"); // message is sent to receiver "pd", prepended by the string "dsp"
    }

    void run(uint32_t nframes) {
      mFrameTime = 0;
      with_lock([this, nframes] () {
        current_plugin = this; //for floathook
        setup_midi_out();
        for (auto& kv: mControlIn) {
          std::string ctrl_name = kv.second;
          float value = *p(kv.first);
          call_pd<int, const char *, float>(mLIBPDHandle, "libpd_float", ctrl_name.c_str(), value);
        }

        for (auto& kv: mMIDIIn) {
          const LV2_Atom_Sequence* midibuf = p<LV2_Atom_Sequence>(kv.first);
          handle_midi_in(midibuf);
        }

        //XXX need to juggle between lv2 frames and pd blocks because libpd_process_raw expects nchannels * block_size length arrays

        //XXX pd block size has to be an equal divisor of nframes
        float * in_buf = &mPDInputBuffer.front();
        float * out_buf = &mPDOutputBuffer.front();
        for (uint32_t i = 0; i < nframes; i += mPDBlockSize) {
          for (uint32_t c = 0; c < mAudioIn.size(); c++)
            memcpy(in_buf + c * mPDBlockSize, p(mAudioIn[c]) + i, mPDBlockSize * sizeof(float));

          memset(out_buf, 0, mPDOutputBuffer.size() * sizeof(float));
          call_pd<int, const float *, const float *>(mLIBPDHandle, "libpd_process_raw", in_buf, out_buf);

          for (uint32_t c = 0; c < mAudioOut.size(); c++)
            memcpy(p(mAudioOut[c]) + i, out_buf + c * mPDBlockSize, mPDBlockSize * sizeof(float));
          mFrameTime = i;
        }
        complete_midi_out();
        current_plugin = nullptr;
      });
    }

    void setup_midi_out() {
      for (auto& kv: mMIDIOut) {
        LV2_Atom_Sequence* midibuf = p<LV2_Atom_Sequence>(kv.first);
        uint32_t capacity = midibuf->atom.size;

        lv2_atom_forge_set_buffer(&kv.second.forge, (uint8_t *)midibuf, capacity);
        lv2_atom_forge_sequence_head(&kv.second.forge, &kv.second.frame, 0);
      }
    }

    void complete_midi_out() {
      for (auto& kv: mMIDIOut) {
        lv2_atom_forge_pop(&kv.second.forge, &kv.second.frame);
      }
    }

    void handle_midi_in(const LV2_Atom_Sequence* midibuf) {
      LV2_ATOM_SEQUENCE_FOREACH(midibuf, ev) {
        if (ev->body.type == mIds.midi_event) {
          //the actual data is stored after the event
          const uint8_t* const msg = (const uint8_t*)(ev + 1);
          for (uint32_t i = 0; i < ev->body.size; i++) {
            call_pd<int, int, int>(mLIBPDHandle, "libpd_midibyte", 0, static_cast<int>(msg[i]));
          }
          switch (lv2_midi_message_type(msg)) {
            case LV2_MIDI_MSG_INVALID:
              break;
            case LV2_MIDI_MSG_NOTE_OFF:
              call_pd<int, int, int, int>(mLIBPDHandle,
                  "libpd_noteon", static_cast<int>(CHAN_MASK & msg[0]), static_cast<int>(msg[1]), 0);
              break;
            case LV2_MIDI_MSG_NOTE_ON:
              call_pd<int, int, int, int>(mLIBPDHandle,
                  "libpd_noteon", static_cast<int>(CHAN_MASK & msg[0]), static_cast<int>(msg[1]), static_cast<int>(msg[2]));
              break;
            case LV2_MIDI_MSG_NOTE_PRESSURE:
              call_pd<int, int, int, int>(mLIBPDHandle,
                  "libpd_polyaftertouch", static_cast<int>(CHAN_MASK & msg[0]), static_cast<int>(msg[1]), static_cast<int>(msg[2]));
              break;
            case LV2_MIDI_MSG_CONTROLLER:
              call_pd<int, int, int, int>(mLIBPDHandle,
                  "libpd_controlchange", static_cast<int>(CHAN_MASK & msg[0]), static_cast<int>(msg[1]), static_cast<int>(msg[2]));
              break;
            case LV2_MIDI_MSG_PGM_CHANGE:
              call_pd<int, int, int>(mLIBPDHandle,
                  "libpd_programchange", static_cast<int>(CHAN_MASK & msg[0]), static_cast<int>(msg[1]));
              break;
            case LV2_MIDI_MSG_CHANNEL_PRESSURE:
              call_pd<int, int, int>(mLIBPDHandle,
                  "libpd_aftertouch", static_cast<int>(CHAN_MASK & msg[0]), static_cast<int>(msg[1]));
              break;
            case LV2_MIDI_MSG_BENDER:
              {
                int value = ((static_cast<uint16_t>(msg[2]) << 7) | msg[1]) - 8192;
                call_pd<int, int, int>(mLIBPDHandle,
                    "libpd_pitchbend", static_cast<int>(CHAN_MASK & msg[0]), value);
              }
              break;
            case LV2_MIDI_MSG_SYSTEM_EXCLUSIVE:
              //XXX TODO
              break;
            case LV2_MIDI_MSG_MTC_QUARTER:
              break;
            case LV2_MIDI_MSG_SONG_POS:
              break;
            case LV2_MIDI_MSG_SONG_SELECT:
              break;
            case LV2_MIDI_MSG_TUNE_REQUEST:
              break;
            case LV2_MIDI_MSG_CLOCK:
            case LV2_MIDI_MSG_START:
            case LV2_MIDI_MSG_CONTINUE:
            case LV2_MIDI_MSG_STOP:
            case LV2_MIDI_MSG_ACTIVE_SENSE:
            case LV2_MIDI_MSG_RESET:
              call_pd<int, int, int>(mLIBPDHandle,
                  "libpd_sysrealtime", 0, static_cast<int>(msg[0]));
              break;
          }
        }
      }
    }

    struct midi_out_data_t {
      LV2_Atom_Forge_Frame frame;
      LV2_Atom_Forge forge;
    };
    //time within the current processing frame.. used for event offsets
    int64_t mFrameTime = 0;
    std::map<uint32_t, std::string> mControlIn;
    std::map<uint32_t, std::string> mControlOut;
    std::map<uint32_t, std::string> mMIDIIn;
    std::map<uint32_t, midi_out_data_t> mMIDIOut;
    std::vector<uint32_t> mAudioIn;
    std::vector<uint32_t> mAudioOut;
    uint32_t mPDDollarZero = 0;
    size_t mPDBlockSize = 64;
    std::vector<float> mPDInputBuffer;
    std::vector<float> mPDOutputBuffer;
    void * mLIBPDHandle = nullptr;
    std::string mLIBPDUniquePath;
    void * mPatchFileHandle = nullptr;

    struct mapped_ids {
      LV2_URID midi_event;
    };
    mapped_ids mIds;
};

namespace {
  void pd_floathook(const char *s, float value) {
    if (current_plugin)
      current_plugin->process_float(std::string(s), value);
  }
  void pd_noteonhook(int channel, int pitch, int velocity) {
    if (current_plugin)
      current_plugin->process_noteon(channel, pitch, velocity);
  }
  void pd_controlchangehook(int channel, int controller, int value) {
    if (current_plugin)
      current_plugin->process_cc(channel, controller, value);
  }
  void pd_programchangehook(int channel, int value) {
    if (current_plugin)
      current_plugin->process_pgrmchg(channel, value);
  }
  void pd_pitchbendhook(int channel, int value) {
    if (current_plugin)
      current_plugin->process_bend(channel, value);
  }
  void pd_aftertouchhook(int channel, int value) {
    if (current_plugin)
      current_plugin->process_touch(channel, value);
  }
  void pd_polyaftertouchhook(int channel, int pitch, int value) {
    if (current_plugin)
      current_plugin->process_poly_touch(channel, pitch, value);
  }
  //redundant? void pd_midibytehook(int port, int byte) { }
}

static int _ = PDLv2Plugin::register_class(pdlv2::plugin_uri);
