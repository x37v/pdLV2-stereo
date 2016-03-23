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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>

using namespace LV2;
using std::cout;
using std::cerr;
using std::endl;
namespace sp = std::placeholders;

class PDLv2Plugin;
namespace {
  template<typename R, typename T0>
    R call_pd(void * library_handle, std::string func_name, T0 arg0) {
      R (*ptr)(T0);
      ptr = (R (*)(T0))dlsym(library_handle, func_name.c_str());
      if (!ptr) {
        cerr << "couldn't get function " << func_name << endl;
        return R();
      }
      return (*ptr)(arg0);
    }

  template<typename R, typename T0, typename T1>
    R call_pd(void * library_handle, std::string func_name, T0 arg0, T1 arg1) {
      R (*ptr)(T0, T1);
      ptr = (R (*)(T0, T1))dlsym(library_handle, func_name.c_str());
      if (!ptr) {
        cerr << "couldn't get function " << func_name << endl;
        return R();
      }
      return (*ptr)(arg0, arg1);
    }

  template<typename R, typename T0, typename T1, typename T2>
    R call_pd(void * library_handle, std::string func_name, T0 arg0, T1 arg1, T2 arg2) {
      R (*ptr)(T0, T1, T2);
      ptr = (R (*)(T0, T1, T2))dlsym(library_handle, func_name.c_str());
      if (!ptr) {
        cerr << "couldn't get function " << func_name << endl;
        return R();
      }
      return (*ptr)(arg0, arg1, arg2);
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

  template<typename T0>
    void call_pd_ret_void(void * library_handle, std::string func_name, T0 arg0) {
      void (*ptr)(T0);
      ptr = (void (*)(T0))dlsym(library_handle, func_name.c_str());
      if (!ptr) {
        cerr << "couldn't get function " << func_name << endl;
        return;
      }
      (*ptr)(arg0);
    }

  template<typename T0, typename T1>
    void call_pd_ret_void(void * library_handle, std::string func_name, T0 arg0, T1 arg1) {
      void (*ptr)(T0, T1);
      ptr = (void (*)(T0, T1))dlsym(library_handle, func_name.c_str());
      if (!ptr) {
        cerr << "couldn't get function " << func_name << endl;
        return;
      }
      (*ptr)(arg0, arg1);
    }

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
    void * mLIBPDHandle = nullptr;
  public:
    PDLv2Plugin(double rate) : Plugin<PDLv2Plugin>(pdlv2::ports.size()) {
      const std::string plugin_bundle_path(bundle_path());
      std::string so_path = plugin_bundle_path + "/libpd.so";
      //mLIBPDHandle = dlmopen(LM_ID_NEWLM, so_path.c_str(), RTLD_NOW);
      //mLIBPDHandle = dlopen(so_path.c_str(), RTLD_NOW); //works
      mLIBPDHandle = dlopen(so_path.c_str(), RTLD_NOW | RTLD_DEEPBIND | RTLD_LOCAL); //fucked up sound with multiple copies of same plugin
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
      //libpd_set_printhook((t_libpd_printhook) pdprint);

      call_pd_void<int>(mLIBPDHandle, "libpd_init");
      /*
      //libpd_init();
      int (*iptr)(void);
      iptr = (int (*)(void))dlsym(mLIBPDHandle, "libpd_init");
      (*iptr);
      */

      //libpd_init_audio(mAudioIn.size(), mAudioOut.size(), static_cast<int>(rate)); 
      call_pd<int, int, int, int>(mLIBPDHandle, "libpd_init_audio", mAudioIn.size(), mAudioOut.size(), static_cast<int>(rate)); 
      //libpd_set_floathook(&pd_floathook);
      call_pd_ret_void<const t_libpd_floathook>(mLIBPDHandle, "libpd_set_floathook", &pd_floathook);

      //void *fileHandle = libpd_openfile(pdlv2::patch_file_name, plugin_bundle_path.c_str()); // open patch   [; pd open file folder(
      void *fileHandle = call_pd<void *, const char *, const char *>(mLIBPDHandle, "libpd_openfile", pdlv2::patch_file_name, plugin_bundle_path.c_str()); // open patch   [; pd open file folder(
      //mPDDollarZero = libpd_getdollarzero(fileHandle); // get dollarzero from patch
      mPDDollarZero = call_pd<int, void *>(mLIBPDHandle, "libpd_getdollarzero", fileHandle); // get dollarzero from patch
      //mPDBlockSize = libpd_blocksize();
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
            //libpd_bind(mControlOut[i].c_str());
            call_pd<void*, const char *>(mLIBPDHandle, "libpd_bind", mControlOut[i].c_str());
            break;
        }
      }

      mPDInputBuffer.resize(mPDBlockSize * mAudioIn.size());
      mPDOutputBuffer.resize(mPDBlockSize * mAudioOut.size());
    }

    virtual ~PDLv2Plugin() {
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
      call_pd<int, int>(mLIBPDHandle, "libpd_start_message", 1);  // begin of message
      call_pd_ret_void<float>(mLIBPDHandle, "libpd_add_float", 1.0f);  // message contains now "1"
      call_pd<int, const char *, const char *>(mLIBPDHandle, "libpd_finish_message", "pd", "dsp"); // message is sent to receiver "pd", prepended by the string "dsp"
    }

    void run(uint32_t nframes) {
      current_plugin = this; //for floathook

      for (auto& kv: mControlIn) {
        std::string ctrl_name = kv.second;
        float value = *p(kv.first);
        call_pd<int, const char *, float>(mLIBPDHandle, "libpd_float", ctrl_name.c_str(), value);
      }

      //XXX need to juggle between lv2 frames and pd blocks because libpd_process_raw expects nchannels * block_size length arrays

      //XXX pd block size has to be an equal divisor of nframes
      float * in_buf = &mPDInputBuffer.front();
      float * out_buf = &mPDOutputBuffer.front();
      for (uint32_t i = 0; i < nframes; i += mPDBlockSize) {
        for (uint32_t c = 0; c < mAudioIn.size(); c++)
          memcpy(in_buf + c * mPDBlockSize, p(mAudioIn[c]) + i, mPDBlockSize * sizeof(float));

        memset(out_buf, 0, mPDOutputBuffer.size() * sizeof(float));
        //libpd_process_raw(in_buf, out_buf);
        call_pd<int, const float *, const float *>(mLIBPDHandle, "libpd_process_raw", in_buf, out_buf);

        for (uint32_t c = 0; c < mAudioOut.size(); c++)
          memcpy(p(mAudioOut[c]) + i, out_buf + c * mPDBlockSize, mPDBlockSize * sizeof(float));
      }
      current_plugin = nullptr;
    }

    std::map<uint32_t, std::string> mControlIn;
    std::map<uint32_t, std::string> mControlOut;
    std::vector<uint32_t> mAudioIn;
    std::vector<uint32_t> mAudioOut;
    uint32_t mPDDollarZero = 0;
    size_t mPDBlockSize = 64;
    std::vector<float> mPDInputBuffer;
    std::vector<float> mPDOutputBuffer;
};

namespace {
  void pd_floathook(const char *s, float value) {
    if (current_plugin)
      current_plugin->process_float(std::string(s), value);
  }
}

static int _ = PDLv2Plugin::register_class(pdlv2::plugin_uri);
