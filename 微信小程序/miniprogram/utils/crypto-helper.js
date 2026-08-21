/**
 * crypto-helper.js
 * 纯 JavaScript 实现的 SHA1、HMAC-SHA1、Base64 编解码
 * 用于微信小程序生成 OneNET 鉴权 Token
 */

// ==================== Base64 ====================

const BASE64_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

function base64Encode(bytes) {
  let result = '';
  const len = bytes.length;
  for (let i = 0; i < len; i += 3) {
    const b1 = bytes[i];
    const b2 = i + 1 < len ? bytes[i + 1] : 0;
    const b3 = i + 2 < len ? bytes[i + 2] : 0;
    result += BASE64_CHARS[b1 >> 2];
    result += BASE64_CHARS[((b1 & 0x03) << 4) | (b2 >> 4)];
    result += i + 1 < len ? BASE64_CHARS[((b2 & 0x0F) << 2) | (b3 >> 6)] : '=';
    result += i + 2 < len ? BASE64_CHARS[b3 & 0x3F] : '=';
  }
  return result;
}

function base64Decode(str) {
  str = (str || '').replace(/[^A-Za-z0-9\+\/]/g, '');
  const bytes = [];
  const len = str.length;
  for (let i = 0; i < len; i += 4) {
    const idx1 = BASE64_CHARS.indexOf(str[i] || 'A');
    const idx2 = BASE64_CHARS.indexOf(str[i + 1] || 'A');
    const idx3 = i + 2 < len ? BASE64_CHARS.indexOf(str[i + 2]) : -1;
    const idx4 = i + 3 < len ? BASE64_CHARS.indexOf(str[i + 3]) : -1;
    bytes.push((idx1 << 2) | (idx2 >> 4));
    if (idx3 !== -1) bytes.push(((idx2 & 0x0F) << 4) | (idx3 >> 2));
    if (idx4 !== -1) bytes.push(((idx3 & 0x03) << 6) | idx4);
  }
  return bytes;
}

// ==================== SHA1 ====================

function leftRotate(value, count) {
  return ((value << count) | (value >>> (32 - count))) >>> 0;
}

function sha1(messageBytes) {
  const h = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0];

  // 复制输入
  const msg = messageBytes.slice();
  const msgBitLen = msg.length * 8;

  // 填充: 先加 0x80
  msg.push(0x80);

  // 填充 0 直到 (长度 % 64) == 56 （即位数 % 512 == 448）
  while ((msg.length % 64) !== 56) {
    msg.push(0);
  }

  // 追加 64 位大端序原始长度（避免浮点精度问题，分离低 32 位）
  var lenHi = Math.floor(msgBitLen / 0x100000000);
  var lenLo = msgBitLen >>> 0;
  msg.push((lenHi >>> 24) & 0xFF);
  msg.push((lenHi >>> 16) & 0xFF);
  msg.push((lenHi >>> 8) & 0xFF);
  msg.push(lenHi & 0xFF);
  msg.push((lenLo >>> 24) & 0xFF);
  msg.push((lenLo >>> 16) & 0xFF);
  msg.push((lenLo >>> 8) & 0xFF);
  msg.push(lenLo & 0xFF);

  // 处理每个 512 位块
  for (let i = 0; i < msg.length; i += 64) {
    const w = new Array(80);

    for (let t = 0; t < 16; t++) {
      w[t] = (msg[i + t * 4] << 24) | (msg[i + t * 4 + 1] << 16) |
             (msg[i + t * 4 + 2] << 8) | msg[i + t * 4 + 3];
    }

    for (let t = 16; t < 80; t++) {
      w[t] = leftRotate(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
    }

    let a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];

    for (let t = 0; t < 80; t++) {
      let f, k;
      if (t < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999;
      } else if (t < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (t < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }

      const temp = (leftRotate(a, 5) + f + e + k + w[t]) >>> 0;
      e = d;
      d = c;
      c = leftRotate(b, 30);
      b = a;
      a = temp;
    }

    h[0] = (h[0] + a) >>> 0;
    h[1] = (h[1] + b) >>> 0;
    h[2] = (h[2] + c) >>> 0;
    h[3] = (h[3] + d) >>> 0;
    h[4] = (h[4] + e) >>> 0;
  }

  // 转为字节数组
  const result = [];
  for (let i = 0; i < 5; i++) {
    result.push((h[i] >>> 24) & 0xFF);
    result.push((h[i] >>> 16) & 0xFF);
    result.push((h[i] >>> 8) & 0xFF);
    result.push(h[i] & 0xFF);
  }
  return result;
}

// ==================== Hex 编解码 ====================

function hexDecode(str) {
  str = (str || '').replace(/[^0-9a-fA-F]/g, '');
  const bytes = [];
  for (let i = 0; i < str.length; i += 2) {
    bytes.push(parseInt(str.substring(i, i + 2), 16));
  }
  return bytes;
}

// ==================== MD5 ====================

var MD5_S = [
  7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
  5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
  4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
  6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
];

var MD5_K = [];
for (var i = 0; i < 64; i++) {
  MD5_K[i] = Math.floor(Math.abs(Math.sin(i + 1)) * 0x100000000) >>> 0;
}

function md5(messageBytes) {
  var a0 = 0x67452301;
  var b0 = 0xEFCDAB89;
  var c0 = 0x98BADCFE;
  var d0 = 0x10325476;

  var msg = messageBytes.slice();
  var msgBitLen = msg.length * 8;

  // 填充
  msg.push(0x80);
  while ((msg.length % 64) !== 56) {
    msg.push(0);
  }

  // 追加 64 位小端序长度
  var lenLo = msgBitLen >>> 0;
  var lenHi = Math.floor(msgBitLen / 0x100000000);
  msg.push(lenLo & 0xFF, (lenLo >>> 8) & 0xFF, (lenLo >>> 16) & 0xFF, (lenLo >>> 24) & 0xFF);
  msg.push(lenHi & 0xFF, (lenHi >>> 8) & 0xFF, (lenHi >>> 16) & 0xFF, (lenHi >>> 24) & 0xFF);

  for (var offset = 0; offset < msg.length; offset += 64) {
    var M = [];
    for (var t = 0; t < 16; t++) {
      M[t] = msg[offset + t * 4] | (msg[offset + t * 4 + 1] << 8) |
             (msg[offset + t * 4 + 2] << 16) | (msg[offset + t * 4 + 3] << 24);
    }

    var A = a0, B = b0, C = c0, D = d0;

    for (var t = 0; t < 64; t++) {
      var F, g;
      if (t < 16) {
        F = (B & C) | (~B & D);
        g = t;
      } else if (t < 32) {
        F = (D & B) | (~D & C);
        g = (5 * t + 1) % 16;
      } else if (t < 48) {
        F = B ^ C ^ D;
        g = (3 * t + 5) % 16;
      } else {
        F = C ^ (B | ~D);
        g = (7 * t) % 16;
      }
      F = (F + A + MD5_K[t] + M[g]) >>> 0;
      A = D;
      D = C;
      C = B;
      B = (B + ((F << MD5_S[t]) | (F >>> (32 - MD5_S[t])))) >>> 0;
    }

    a0 = (a0 + A) >>> 0;
    b0 = (b0 + B) >>> 0;
    c0 = (c0 + C) >>> 0;
    d0 = (d0 + D) >>> 0;
  }

  // 小端序输出
  var result = [];
  [a0, b0, c0, d0].forEach(function (val) {
    result.push(val & 0xFF, (val >>> 8) & 0xFF, (val >>> 16) & 0xFF, (val >>> 24) & 0xFF);
  });
  return result;
}

// ==================== HMAC ====================

function hmac(hashFn, blockSize, keyBytes, message) {
  var key = keyBytes.slice();

  if (key.length > blockSize) {
    key = hashFn(key);
  }
  while (key.length < blockSize) {
    key.push(0);
  }

  var ipad = key.map(function (b) { return b ^ 0x36; });
  var opad = key.map(function (b) { return b ^ 0x5C; });

  var msgBytes = [];
  for (var i = 0; i < message.length; i++) {
    var code = message.charCodeAt(i);
    if (code < 0x80) {
      msgBytes.push(code);
    } else if (code < 0x800) {
      msgBytes.push(0xC0 | (code >> 6));
      msgBytes.push(0x80 | (code & 0x3F));
    } else {
      msgBytes.push(0xE0 | (code >> 12));
      msgBytes.push(0x80 | ((code >> 6) & 0x3F));
      msgBytes.push(0x80 | (code & 0x3F));
    }
  }

  var inner = hashFn(ipad.concat(msgBytes));
  var outer = hashFn(opad.concat(inner));
  return outer;
}

function hmacSha1(keyBytes, message) {
  return hmac(sha1, 64, keyBytes, message);
}

function hmacMd5(keyBytes, message) {
  return hmac(md5, 64, keyBytes, message);
}

// ==================== 导出 ====================

module.exports = {
  sha1: sha1,
  md5: md5,
  hmacSha1: hmacSha1,
  hmacMd5: hmacMd5,
  base64Encode: base64Encode,
  base64Decode: base64Decode,
  hexDecode: hexDecode
};
