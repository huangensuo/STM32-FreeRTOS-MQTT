#!/usr/bin/env node

const crypto = require('crypto');

function parseArgs(argv) {
  const args = {};
  for (let i = 2; i < argv.length; i++) {
    const item = argv[i];
    if (!item.startsWith('--')) continue;
    const key = item.slice(2);
    const next = argv[i + 1];
    if (!next || next.startsWith('--')) {
      args[key] = true;
    } else {
      args[key] = next;
      i++;
    }
  }
  return args;
}

function usage() {
  console.log('Usage:');
  console.log('  node tools/generate-onenet-token.js --product PRODUCT_ID --device DEVICE_NAME --accessKey ACCESS_KEY [--user USER_ID] [--days 3650]');
  console.log('');
  console.log('Example:');
  console.log('  node tools/generate-onenet-token.js --product PB8XHZe391 --device stm32_env_monitor --accessKey YOUR_ACCESS_KEY --user 526797');
}

function decodeAccessKey(accessKey) {
  if (!accessKey) return null;
  if (/^[0-9a-fA-F]+$/.test(accessKey) && accessKey.length % 2 === 0) {
    return Buffer.from(accessKey, 'hex');
  }
  return Buffer.from(accessKey, 'base64');
}

function encodeTokenPart(value) {
  return encodeURIComponent(value).replace(/\(/g, '%28').replace(/\)/g, '%29');
}

function signToken(accessKey, method, res, version, et) {
  const key = decodeAccessKey(accessKey);
  if (!key || key.length === 0) {
    throw new Error('Missing or invalid accessKey');
  }

  const signStr = et + '\n' + method + '\n' + res + '\n' + version;
  const hmac = crypto.createHmac(method, key);
  hmac.update(signStr);

  return 'version=' + version +
    '&res=' + encodeTokenPart(res) +
    '&et=' + et +
    '&method=' + method +
    '&sign=' + encodeTokenPart(hmac.digest('base64'));
}

const args = parseArgs(process.argv);
if (args.help || !args.product || !args.device || !args.accessKey) {
  usage();
  process.exit(args.help ? 0 : 1);
}

const days = Number(args.days || 3650);
const et = Math.floor(Date.now() / 1000) + Math.floor(days * 86400);
const productRes = 'products/' + args.product;
const deviceRes = productRes + '/devices/' + args.device;

console.log('PRODUCT_ID=' + args.product);
console.log('DEVICE_NAME=' + args.device);
console.log('EXPIRE_AT_UNIX=' + et);
console.log('');
console.log('MQTT_PASSWORD=' + signToken(args.accessKey, 'sha1', deviceRes, '2018-10-31', et));
console.log('');
console.log('API_AUTH_PRODUCT=' + signToken(args.accessKey, 'sha1', productRes, '2018-10-31', et));
console.log('API_AUTH_DEVICE=' + signToken(args.accessKey, 'sha1', deviceRes, '2018-10-31', et));

if (args.user) {
  console.log('API_AUTH_USER=' + signToken(args.accessKey, 'md5', 'userid/' + args.user, '2022-05-01', et));
}

