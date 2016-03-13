#include <lv2plugin.hpp>
#include <iostream>
#include <libpd/z_libpd.h>

using namespace LV2;

namespace xnor {
  enum port_type_t {
    AUDIO_IN,
    AUDIO_OUT,
    CONTROL_IN,
    CONTROL_OUT
  };

  struct PortInfo {
    port_type_t type;
    std::string name;
    PortInfo(
        port_type_t _type,
        std::string _name) : type(_type), name(_name) {}
  };

  void pdprint(const char *s) {
    std::cout << s << std::endl;
  }

  const char * patch_file_name = "stereopanner.pd";

  const std::vector<PortInfo> ports = {
    {CONTROL_IN, "width"},
    {CONTROL_OUT, "balance"},
    {AUDIO_IN, "left_input"},
    {AUDIO_IN, "right_input"},
    {AUDIO_OUT, "left_output"},
    {AUDIO_OUT, "right_output"},
  };
}

class XNORLv2Plugin : public Plugin<XNORLv2Plugin> {
  public:
    XNORLv2Plugin(double rate) : Plugin<XNORLv2Plugin>(xnor::ports.size()) {
      for (size_t i = 0; i < xnor::ports.size(); i++) {
        xnor::PortInfo info = xnor::ports.at(i);
        switch (info.type) {
          case xnor::AUDIO_IN:
            mAudioIn[i] = info.name;
            break;
          case xnor::AUDIO_OUT:
            mAudioOut[i] = info.name;
            break;
          case xnor::CONTROL_IN:
            mControlIn[i] = info.name;
            break;
          case xnor::CONTROL_OUT:
            mControlOut[i] = info.name;
            break;
        }
      }

      if (libpd_exists("libpdIsRunning") == 0) {
        libpd_set_printhook((t_libpd_printhook) xnor::pdprint);
        libpd_init();
        libpd_init_audio(mAudioIn.size(), mAudioOut.size(), static_cast<int>(rate)); 
        libpd_bind("libpdIsRunning");
      }
    }

    void activate() {
      void *fileHandle = libpd_openfile(xnor::patch_file_name, bundle_path()); // open patch   [; pd open file folder(
      mPDDollarZero = libpd_getdollarzero(fileHandle); // get dollarzero from patch
      mPDBlockSize = libpd_blocksize();

      mPDInputBuffer.resize(mPDBlockSize * mAudioIn.size());
      mPDOutputBuffer.resize(mPDBlockSize * mAudioOut.size());

      libpd_start_message(1);  // begin of message
      libpd_add_float(1.0f);  // message contains now "1"
      libpd_finish_message("pd", "dsp"); // message is sent to receiver "pd", prepended by the string "dsp"
    }

    void run(uint32_t nframes) {
      for (auto& kv: mControlIn) {
        //XXX can optimize this
        std::string ctrl_name = std::to_string(mPDDollarZero) + "-" + kv.second;
        float value = *p(kv.first);
        libpd_float(ctrl_name.c_str(), value);
      }
      
      //XXX need to juggle between lv2 frames and pd blocks becuase libpd_process_raw expects nchannels * block_size length arrays

      for (uint32_t i = 0; i < nframes; ++i)
        p(0)[i] = 0;
    }
  private:
    std::map<uint32_t, std::string> mControlIn;
    std::map<uint32_t, std::string> mControlOut;
    std::map<uint32_t, std::string> mAudioIn;
    std::map<uint32_t, std::string> mAudioOut;
    uint32_t mPDDollarZero = 0;
    size_t mPDBlockSize = 64;
    std::vector<float> mPDInputBuffer;
    std::vector<float> mPDOutputBuffer;
    size_t mPDBufferIndex = 0;
};

static int _ = XNORLv2Plugin::register_class("http://xnor.info/lv2/stereopanner");
