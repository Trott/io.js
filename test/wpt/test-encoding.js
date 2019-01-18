'use strict';

const common = require('../common');
common.requireFlags('--expose-internals');
const { WPTRunner } = require('../common/wpt');
const runner = new WPTRunner('encoding');

// Copy global descriptors from the global object
runner.copyGlobalsFromObject(global, ['TextDecoder', 'TextEncoder']);

runner.runJsTests();
