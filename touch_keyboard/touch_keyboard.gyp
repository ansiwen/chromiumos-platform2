{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
      ],
    },
  },

  'targets': [
    {
      'target_name': 'touch_keyboard_handler',
      'type': 'executable',
      'sources': [
        'evdevsource.cc',
        'fakekeyboard.cc',
        'faketouchpad.cc',
        'haptic/ff_driver.cc',
        'haptic/touch_ff_manager.cc',
        'main.cc',
        'statemachine/eventkey.cc',
        'statemachine/slot.cc',
        'statemachine/statemachine.cc',
        'uinputdevice.cc',
      ],
    },
    {
      'target_name': 'touchkb_haptic_test',
      'type': 'executable',
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
        ],
      },
      'sources': [
        'haptic/ff_driver.cc',
        'haptic/haptic_test.cc',
      ],
    },
  ],

  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'eventkey_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'statemachine/eventkey.cc',
            'statemachine/eventkey_test.cc',
          ],
        },
        {
          'target_name': 'slot_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'statemachine/eventkey.cc',
            'statemachine/slot.cc',
            'statemachine/slot_test.cc',
          ],
        },
        {
          'target_name': 'statemachine_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'statemachine/eventkey.cc',
            'statemachine/slot.cc',
            'statemachine/statemachine.cc',
            'statemachine/statemachine_test.cc',
          ],
        },
        {
          'target_name': 'evdevsource_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'evdevsource.cc',
            'evdevsource_test.cc',
          ],
        },
        {
          'target_name': 'uinputdevice_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'uinputdevice.cc',
            'uinputdevice_test.cc',
          ],
        },
      ],
    }],
  ],
}
