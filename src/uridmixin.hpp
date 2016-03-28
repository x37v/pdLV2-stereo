#pragma once

/*
  written by Alex Norman 2016, derived from:
  
  lv2plugin.hpp - support file for writing LV2 plugins in C++
  Copyright (C) 2006-2007 Lars Luthman <lars.luthman@gmail.com>
  Modified by Dave Robillard, 2008
*/

#include <lv2/lv2plug.in/ns/ext/urid/urid.h>

// this implements a mixin for the URID map extension and provides
// a method to map URIs [strings] to URID [numbers]

namespace LV2 {
template <bool Required = true>
struct URID {
  template <class Derived>
  struct I : Extension<Required> {
    /** @internal */
    I() {}

    /** @internal */
    static void map_feature_handlers(FeatureHandlerMap& hmap) {
      hmap[LV2_URID__map] = &I<Derived>::handle_feature;
    }

    /** @internal */
    static void handle_feature(void* instance, void* data) {
      Derived* d = reinterpret_cast<Derived*>(instance);
      I<Derived>* fe = static_cast<I<Derived>*>(d);
      fe->m_map = (LV2_URID_Map*)data;
      fe->m_ok = fe->m_map != NULL;
    }

   protected:
    LV2_URID_Map* m_map = nullptr;
    LV2_URID map_uri(const char* uri) {
      if (m_map == nullptr) return 0;
      return m_map->map(m_map->handle, uri);
    }
  };
};
}
