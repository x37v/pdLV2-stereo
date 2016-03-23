namespace pdlv2 {
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

  const char * patch_file_name = "host.pd";
}
