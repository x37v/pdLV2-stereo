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

#include "plugin.h"

using namespace LV2;
using std::cout;
using std::endl;
namespace sp = std::placeholders;

class PDLv2Plugin;
namespace {
  PDLv2Plugin * current_plugin = nullptr;

  std::atomic_flag pd_global_lock = ATOMIC_FLAG_INIT;

  void pdprint(const char *s) {
    cout << s;
  }

  void pd_floathook(const char *s, float value);

  void with_lock(std::function<void()> func) {
    while (pd_global_lock.test_and_set(std::memory_order_acquire));  // spin, acquire lock
    func();
    pd_global_lock.clear(std::memory_order_release); // release lock
  }

  //spin lock
  void with_instance(t_pdinstance * instance, std::function<void()> func) {
    while (pd_global_lock.test_and_set(std::memory_order_acquire));  // spin, acquire lock
    pd_setinstance(instance);
    func();
    pd_global_lock.clear(std::memory_order_release); // release lock
  }
}

class PDLv2Plugin :
  public Plugin<PDLv2Plugin>
{
  private:
    void float_message_callback(const char *source, float value) {
    }
  public:
    PDLv2Plugin(double rate) : Plugin<PDLv2Plugin>(pdlv2::ports.size()) {
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

      with_lock([this]() {
        mPDInstance = pdinstance_new();
      });
      with_instance(mPDInstance, [this, rate]() {
        libpd_set_printhook((t_libpd_printhook) pdprint);
        libpd_init();
        libpd_init_audio(mAudioIn.size(), mAudioOut.size(), static_cast<int>(rate)); 
        libpd_set_floathook(&pd_floathook);

        void *fileHandle = libpd_openfile(pdlv2::patch_file_name, bundle_path()); // open patch   [; pd open file folder(
        mPDDollarZero = libpd_getdollarzero(fileHandle); // get dollarzero from patch
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
          }
        }
      });

      mPDInputBuffer.resize(mPDBlockSize * mAudioIn.size());
      mPDOutputBuffer.resize(mPDBlockSize * mAudioOut.size());
    }

    virtual ~PDLv2Plugin() {
      with_lock([this]() {
        pdinstance_free(mPDInstance);
      });
    }

    void process_float(std::string prefix, float value) {
      for (auto& kv: mControlOut) {
        if (prefix == kv.second) {
          *p(kv.first) = value;
          break;
        }
      }
    }

    void activate() {
      with_instance(mPDInstance, [this]() {
        libpd_start_message(1);  // begin of message
        libpd_add_float(1.0f);  // message contains now "1"
        libpd_finish_message("pd", "dsp"); // message is sent to receiver "pd", prepended by the string "dsp"
      });
    }

    void run(uint32_t nframes) {
      with_instance(mPDInstance, [this, nframes]() {
        current_plugin = this; //for floathook

        for (auto& kv: mControlIn) {
          std::string ctrl_name = kv.second;
          float value = *p(kv.first);
          libpd_float(ctrl_name.c_str(), value);
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
        }
        current_plugin = nullptr;
      });
    }

    std::map<uint32_t, std::string> mControlIn;
    std::map<uint32_t, std::string> mControlOut;
    std::vector<uint32_t> mAudioIn;
    std::vector<uint32_t> mAudioOut;
    uint32_t mPDDollarZero = 0;
    size_t mPDBlockSize = 64;
    std::vector<float> mPDInputBuffer;
    std::vector<float> mPDOutputBuffer;
    t_pdinstance * mPDInstance;
};

namespace {
  void pd_floathook(const char *s, float value) {
    if (current_plugin)
      current_plugin->process_float(std::string(s), value);
  }
}

static int _ = PDLv2Plugin::register_class(pdlv2::plugin_uri);
