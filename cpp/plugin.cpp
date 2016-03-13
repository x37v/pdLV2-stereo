#include <lv2plugin.hpp>
#include <iostream>
#include <libpd/z_libpd.h>

using namespace LV2;
using std::cout;
using std::endl;

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
    cout << s << endl;
  }

  const char * patch_file_name = "patch.pd";

  const std::vector<PortInfo> ports = {
    {CONTROL_IN, "width"},
    {CONTROL_IN, "balance"},
    {AUDIO_IN, "left_input"},
    {AUDIO_IN, "right_input"},
    {AUDIO_OUT, "left_output"},
    {AUDIO_OUT, "right_output"},
  };
}

class XNORLv2Plugin :
  public Plugin<XNORLv2Plugin>
{
  public:
    XNORLv2Plugin(double rate) : Plugin<XNORLv2Plugin>(xnor::ports.size()) {
      for (size_t i = 0; i < xnor::ports.size(); i++) {
        xnor::PortInfo info = xnor::ports.at(i);
        switch (info.type) {
          case xnor::AUDIO_IN:
            mAudioIn.push_back(i);
            break;
          case xnor::AUDIO_OUT:
            mAudioOut.push_back(i);
            break;
          case xnor::CONTROL_IN:
          case xnor::CONTROL_OUT:
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
      cout << "bundle path: " << bundle_path() << endl;
      std::string directory = "/home/alex/.lv2/lv2x37v-xnor.lv2/";//XXX should just be bundle_path()
      void *fileHandle = libpd_openfile(xnor::patch_file_name, directory.c_str()); // open patch   [; pd open file folder(
      mPDDollarZero = libpd_getdollarzero(fileHandle); // get dollarzero from patch
      mPDBlockSize = libpd_blocksize();

      for (size_t i = 0; i < xnor::ports.size(); i++) {
        xnor::PortInfo info = xnor::ports.at(i);
        switch (info.type) {
          case xnor::AUDIO_IN:
          case xnor::AUDIO_OUT:
            break;
          case xnor::CONTROL_IN:
            mControlIn[i] = std::to_string(mPDDollarZero) + "-" + info.name;
            break;
          case xnor::CONTROL_OUT:
            mControlOut[i] = std::to_string(mPDDollarZero) + "-" + info.name;
            break;
        }
      }

      mPDInputBuffer.resize(mPDBlockSize * mAudioIn.size());
      mPDOutputBuffer.resize(mPDBlockSize * mAudioOut.size());

      libpd_start_message(1);  // begin of message
      libpd_add_float(1.0f);  // message contains now "1"
      libpd_finish_message("pd", "dsp"); // message is sent to receiver "pd", prepended by the string "dsp"
    }

    void run(uint32_t nframes) {
      for (auto& kv: mControlIn) {
        std::string ctrl_name = kv.second;
        float value = *p(kv.first);
        libpd_float(ctrl_name.c_str(), value);
      }
      
      //XXX need to juggle between lv2 frames and pd blocks becuase libpd_process_raw expects nchannels * block_size length arrays

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
    }
  private:
    std::map<uint32_t, std::string> mControlIn;
    std::map<uint32_t, std::string> mControlOut;
    std::vector<uint32_t> mAudioIn;
    std::vector<uint32_t> mAudioOut;
    uint32_t mPDDollarZero = 0;
    size_t mPDBlockSize = 64;
    std::vector<float> mPDInputBuffer;
    std::vector<float> mPDOutputBuffer;
    size_t mPDBufferIndex = 0;
};

static int _ = XNORLv2Plugin::register_class("http://xnor.info/lv2/stereopanner");
