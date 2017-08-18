/*
 * based on work by:
 * Martin Schied https://github.com/unknownError/pdLV2-stereo
 * Lars Luthman http://www.nongnu.org/ll-plugins/lv2pftci/
 */

#include <lvtk/plugin.hpp>
#include <lvtk/ext/atom.hpp>
#include <lvtk/ext/midi.hpp>

#include <iostream>
#include "z_libpd.h"
#include "m_imp.h"
#include <atomic>
#include <functional>
#include <fstream>
#include <cstdio>

#include "plugin.h"

#define CHAN_MASK 0x07

using std::cout;
using std::cerr;
using std::endl;
namespace sp = std::placeholders;

class PDLv2Plugin;
namespace {
  PDLv2Plugin * current_plugin = nullptr;
  bool has_initialized = false;

  std::atomic_flag pd_global_lock = ATOMIC_FLAG_INIT;

  void pdprint(const char *s) {
    cout << "LIBPD: " << s;
  }

  void pd_floathook(const char *s, float value);
  void pd_noteonhook(int channel, int pitch, int velocity);
  void pd_controlchangehook(int channel, int controller, int value);
  void pd_programchangehook(int channel, int value);
  void pd_pitchbendhook(int channel, int value);
  void pd_aftertouchhook(int channel, int value);
  void pd_polyaftertouchhook(int channel, int pitch, int value);
}

class PDLv2Plugin :
  public lvtk::Plugin<PDLv2Plugin, lvtk::URID<true>>
{
  public:
    PDLv2Plugin(double rate) : lvtk::Plugin<PDLv2Plugin, lvtk::URID<true>>(pdlv2::ports.size()) {
      const std::string plugin_bundle_path(bundle_path());

      libpd_set_printhook((t_libpd_printhook)pdprint);
      
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

#if 0
      if (libpd_exists("PDLV2-TEST") != 0) {
        cout << plugin_bundle_path << " EXISTS" << endl;
      } else {
        libpd_init();
        libpd_bind("PDLV2-TEST");
      }
#else
      if (!has_initialized) {
        libpd_init();
        has_initialized = true;
      } else {
        cout << plugin_bundle_path << " EXISTS" << endl;
      }
#endif

      mPDInstance = pdinstance_new();

      pd_setinstance(mPDInstance);
      libpd_init_audio(mAudioIn.size(), mAudioOut.size(), static_cast<int>(rate)); 

      // compute audio    [; pd dsp 1(
      libpd_start_message(1); // one entry in list
      libpd_add_float(1.0f);
      libpd_finish_message("pd", "dsp");

      libpd_set_floathook(&pd_floathook);
      libpd_set_noteonhook(&pd_noteonhook);
      libpd_set_controlchangehook(&pd_controlchangehook);
      libpd_set_programchangehook(&pd_programchangehook);
      libpd_set_pitchbendhook(&pd_pitchbendhook);
      libpd_set_aftertouchhook(&pd_aftertouchhook);
      libpd_set_polyaftertouchhook(&pd_polyaftertouchhook);

      mPatchFileHandle = libpd_openfile(pdlv2::patch_file_name, plugin_bundle_path.c_str());
      mPDDollarZero = libpd_getdollarzero(mPatchFileHandle); // get dollarzero from patch
      mPDBlockSize = libpd_blocksize();

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
            libpd_bind(mControlOut[i].c_str());
            break;
          case pdlv2::MIDI_OUT:
            mMIDIOut[i] = midi_out_data_t(get_urid_map());
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
      libpd_closefile(mPatchFileHandle);
      pdinstance_free(mPDInstance);
    }

    template<std::size_t SIZE>
    void handle_midi_out(std::array<uint8_t, SIZE> midi_bytes) {
      for (auto& kv: mMIDIOut) {
        LV2_Atom_Sequence* midibuf = p<LV2_Atom_Sequence>(kv.first);
        uint32_t capacity = midibuf->atom.size;

        lvtk::AtomForge& f = kv.second.forge;

        f.frame_time(mFrameTime);
        f.write_atom(SIZE, mIds.midi_event);
        f.write_raw(&midi_bytes.front(), SIZE);
        lv2_atom_forge_pad(kv.second.forge.cobj(), SIZE);
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
      mIds.midi_event = map(LV2_MIDI__MidiEvent);
    }

    void run(uint32_t nframes) {
      mFrameTime = 0;
      {
        while (pd_global_lock.test_and_set(std::memory_order_acquire));  // spin, acquire lock
        pd_setinstance(mPDInstance);
        current_plugin = this; //for floathook
        setup_midi_out();
        for (auto& kv: mControlIn) {
          std::string ctrl_name = kv.second;
          float value = *p(kv.first);
          libpd_float(ctrl_name.c_str(), value);
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
          libpd_process_raw(in_buf, out_buf);

          for (uint32_t c = 0; c < mAudioOut.size(); c++)
            memcpy(p(mAudioOut[c]) + i, out_buf + c * mPDBlockSize, mPDBlockSize * sizeof(float));
          mFrameTime = i;
        }
        complete_midi_out();
        current_plugin = nullptr;
        pd_global_lock.clear(std::memory_order_release); // release lock
      };
    }

    void setup_midi_out() {
      for (auto& kv: mMIDIOut) {
        LV2_Atom_Sequence* midibuf = p<LV2_Atom_Sequence>(kv.first);
        uint32_t capacity = midibuf->atom.size;

        lvtk::AtomForge& f = kv.second.forge;
        f.set_buffer((uint8_t*)midibuf, capacity);
        f.sequence_head(kv.second.frame, 0);
      }
    }

    void complete_midi_out() {
      for (auto& kv: mMIDIOut) {
        kv.second.forge.pop(kv.second.frame);
      }
    }

    void handle_midi_in(const LV2_Atom_Sequence* midibuf) {
      LV2_ATOM_SEQUENCE_FOREACH(midibuf, ev) {
        if (ev->body.type == mIds.midi_event) {
          //the actual data is stored after the event
          const uint8_t* const msg = (const uint8_t*)(ev + 1);
          for (uint32_t i = 0; i < ev->body.size; i++) {
            libpd_midibyte(0, static_cast<int>(msg[i]));
          }
          switch (lv2_midi_message_type(msg)) {
            case LV2_MIDI_MSG_INVALID:
              break;
            case LV2_MIDI_MSG_NOTE_OFF:
              libpd_noteon(static_cast<int>(CHAN_MASK & msg[0]), static_cast<int>(msg[1]), 0);
              break;
            case LV2_MIDI_MSG_NOTE_ON:
              libpd_noteon(static_cast<int>(CHAN_MASK & msg[0]), static_cast<int>(msg[1]), static_cast<int>(msg[2]));
              break;
            case LV2_MIDI_MSG_NOTE_PRESSURE:
              libpd_polyaftertouch(static_cast<int>(CHAN_MASK & msg[0]), static_cast<int>(msg[1]), static_cast<int>(msg[2]));
              break;
            case LV2_MIDI_MSG_CONTROLLER:
              libpd_controlchange(static_cast<int>(CHAN_MASK & msg[0]), static_cast<int>(msg[1]), static_cast<int>(msg[2]));
              break;
            case LV2_MIDI_MSG_PGM_CHANGE:
              libpd_programchange(static_cast<int>(CHAN_MASK & msg[0]), static_cast<int>(msg[1]));
              break;
            case LV2_MIDI_MSG_CHANNEL_PRESSURE:
              libpd_aftertouch(static_cast<int>(CHAN_MASK & msg[0]), static_cast<int>(msg[1]));
              break;
            case LV2_MIDI_MSG_BENDER:
              {
                int value = ((static_cast<uint16_t>(msg[2]) << 7) | msg[1]) - 8192;
                libpd_pitchbend(static_cast<int>(CHAN_MASK & msg[0]), value);
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
              libpd_sysrealtime(0, static_cast<int>(msg[0]));
              break;
          }
        }
      }
    }

    struct midi_out_data_t {
      lvtk::ForgeFrame frame;
      lvtk::AtomForge forge;
      midi_out_data_t(LV2_URID_Map* map) : forge(map) { }
      midi_out_data_t() { }
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
    t_pdinstance * mPDInstance;
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
