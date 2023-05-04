'use strict';

const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }
const assert = require('assert');

const opt = require('url').parse('https://httpbin.org/get');
require('https')
  .request(
    { ...opt, path: opt.path + '?A=B' },
    (s) => s.on('data',
                (d) => assert.ok(d.toString('UTF8').includes('https://httpbin.org/get?A=B'))))
  .end();
