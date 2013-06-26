{
  'target_defaults': {
    'dependencies': [
      '../libchromeos/libchromeos-<(libbase_ver).gyp:libchromeos-<(libbase_ver)',
    ],
    'variables': {
      'deps': [
        'dbus-1',
        'dbus-c++-1',
        'glib-2.0',
        'libpcrecpp',
        'libchrome-<(libbase_ver)',
      ],
    },
    "cflags": [
      '-fvisibility=hidden',
    ],
  },
  'targets': [
    {
      'target_name': 'debugd-adaptors',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'adaptor',
        'xml2cpp_out_dir': 'include/adaptors',
      },
      'sources': [
        'share/org.chromium.debugd.xml',
      ],
      'includes': ['../common-mk/xml2cpp.gypi'],
    },
    {
      'target_name': 'debugd-proxies',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'proxy',
        'xml2cpp_in_dir': '<(sysroot)/usr/share/dbus-1/interfaces',
        'xml2cpp_out_dir': 'include/proxies',
      },
      'sources': [
        '<(xml2cpp_in_dir)/org.chromium.flimflam.Device.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.IPConfig.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.Manager.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.Service.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.DBus.Properties.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager1.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.Modem.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.Modem.Simple.xml',
      ],
      'includes': ['../common-mk/xml2cpp.gypi'],
    },
    {
      'target_name': 'libdebugd',
      'type': 'static_library',
      'dependencies': [
        'debugd-proxies',
        'debugd-adaptors',
      ],
      'link_settings': {
        'libraries': [
          '-lminijail',
        ],
      },
      'sources': [
        'src/anonymizer_tool.cc',
        'src/cpu_info_parser.cc',
        'src/debug_daemon.cc',
        'src/debug_logs_tool.cc',
        'src/debug_mode_tool.cc',
        'src/example_tool.cc',
        'src/icmp_tool.cc',
        'src/log_tool.cc',
        'src/memory_tool.cc',
        'src/modem_status_tool.cc',
        'src/netif_tool.cc',
        'src/network_status_tool.cc',
        'src/packet_capture_tool.cc',
        'src/ping_tool.cc',
        'src/perf_tool.cc',
        'src/process_with_id.cc',
        'src/process_with_output.cc',
        'src/random_selector.cc',
        'src/route_tool.cc',
        'src/sandboxed_process.cc',
        'src/storage_tool.cc',
        'src/subprocess_tool.cc',
        'src/sysrq_tool.cc',
        'src/systrace_tool.cc',
        'src/tracepath_tool.cc',
      ],
    },
    {
      'target_name': 'debugd',
      'type': 'executable',
      'dependencies': ['libdebugd'],
      'sources': [
        'src/main.cc',
      ]
    },
    {
      'target_name': 'capture_packets',
      'type': 'executable',
      'libraries': [
        '-lminijail',
        '-lpcap',
      ],
      'sources': [
        'src/helpers/capture_packets.cc',
      ]
    },
    {
      'target_name': 'icmp',
      'type': 'executable',
      'sources': [
        'src/helpers/icmp.cc',
      ]
    },
    {
      'target_name': 'netif',
      'type': 'executable',
      'dependencies': ['debugd-proxies'],
      'sources': [
        'src/helpers/netif.cc',
      ]
    },
    {
      'target_name': 'modem_status',
      'type': 'executable',
      'dependencies': ['debugd-proxies'],
      'sources': [
        'src/helpers/modem_status.cc',
      ]
    },
    {
      'target_name': 'network_status',
      'type': 'executable',
      'dependencies': ['debugd-proxies'],
      'sources': [
        'src/helpers/network_status.cc',
      ]
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'debugd_testrunner',
          'type': 'executable',
          'dependencies': ['libdebugd'],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'src/anonymizer_tool_test.cc',
            'src/log_tool_test.cc',
            'src/modem_status_tool_test.cc',
            'src/process_with_id_test.cc',
            'src/random_selector_test.cc',
            'src/testrunner.cc',
          ]
        },
      ],
    }],
  ],
}
