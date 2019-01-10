// Flags: --inspect=0
'use strict';
const common = require('../common');

// The inspector attempts to start when Node starts. Once started, the inspector
// warns on the use of a SIGPROF listener.

common.skipIfInspectorDisabled();

if (common.isWindows)
  common.skip('test does not apply to Windows');

// Worker inspector never has a server running
if (!require('worker_threads').isMainThread)
  common.skip('This test only works on a main thread');

common.expectWarning('Warning',
                     'process.on(SIGPROF) is reserved while debugging',
                     common.noWarnCode);

process.on('SIGPROF', () => {});
