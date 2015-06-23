{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'openssl',
        'protobuf',
      ]
    },
  },
  'targets': [
    {
      'target_name': 'common',
      'type': 'static_library',
      'sources': [
        'address_mapper.cc',
        'buffer_reader.cc',
        'buffer_writer.cc',
        'data_reader.cc',
        'data_writer.cc',
        'perf_parser.cc',
        'perf_protobuf_io.cc',
        'perf_reader.cc',
        'perf_recorder.cc',
        'perf_serializer.cc',
        'perf_stat_parser.cc',
        'run_command.cc',
        'scoped_temp_path.cc',
        'utils.cc',
      ],
      'dependencies': [
        'perf_data_proto',
        'perf_stat_proto',
      ],
      'export_dependent_settings': [
        'perf_data_proto',
        'perf_stat_proto',
      ],
    },
    {
      'target_name': 'conversion_utils',
      'type': 'static_library',
      'sources': [
        'conversion_utils.cc',
      ],
      'dependencies': [
        'common',
      ],
    },
    {
      'target_name': 'common_test',
      'type': 'static_library',
      'sources': [
        'test_utils.cc',
        'test_perf_data.cc',
      ],
      'dependencies': [
        'common',
      ],
      'export_dependent_settings': [
        'common',
      ],
    },
    {
      'target_name': 'perf_data_proto',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'include',
      },
      'sources': [
        '<(proto_in_dir)/perf_data.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'perf_stat_proto',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'include',
      },
      'sources': [
        '<(proto_in_dir)/perf_stat.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'quipper',
      'type': 'executable',
      'dependencies': [
        'common',
      ],
      'sources': [
        'quipper.cc',
      ]
    },
    {
      'target_name': 'perf_converter',
      'type': 'executable',
      'dependencies': [
        'common',
        'conversion_utils',
      ],
      'sources': [
        'perf_converter.cc',
      ]
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'address_mapper_test',
          'type': 'executable',
          'dependencies': [
            'common',
            'common_test',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'address_mapper_test.cc',
          ]
        },
        {
          'target_name': 'buffer_reader_test',
          'type': 'executable',
          'dependencies': [
            'common',
            'common_test',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'buffer_reader_test.cc',
          ]
        },
        {
          'target_name': 'buffer_writer_test',
          'type': 'executable',
          'dependencies': [
            'common',
            'common_test',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'buffer_writer_test.cc',
          ]
        },
        {
          'target_name': 'conversion_utils_test',
          'type': 'executable',
          'dependencies': [
            'common',
            'common_test',
            'conversion_utils',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'conversion_utils_test.cc',
          ]
        },
        {
          'target_name': 'perf_parser_test',
          'type': 'executable',
          'dependencies': [
            'common',
            'common_test',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'perf_parser_test.cc',
          ]
        },
        {
          'target_name': 'perf_reader_test',
          'type': 'executable',
          'dependencies': [
            'common',
            'common_test',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'perf_reader_test.cc',
          ]
        },
        {
          'target_name': 'perf_recorder_test',
          'type': 'executable',
          'dependencies': [
            'common',
            'common_test',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'perf_recorder_test.cc',
          ]
        },
        {
          'target_name': 'perf_serializer_test',
          'type': 'executable',
          'dependencies': [
            'common',
            'common_test',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'perf_serializer_test.cc',
          ]
        },
        {
          'target_name': 'perf_stat_parser_test',
          'type': 'executable',
          'dependencies': [
            'common',
            'common_test',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'perf_stat_parser_test.cc',
          ]
        },
        {
          'target_name': 'run_command_test',
          'type': 'executable',
          'dependencies': [
            'common',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'run_command_test.cc',
          ]
        },
        {
          'target_name': 'scoped_temp_path_test',
          'type': 'executable',
          'dependencies': [
            'common',
            'common_test',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'scoped_temp_path_test.cc',
          ]
        },
        {
          'target_name': 'utils_test',
          'type': 'executable',
          'dependencies': [
            'common',
            'common_test',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'utils_test.cc',
          ]
        },
      ],
    }],
  ],
}
