package com.antifraud.gateway.udp;

import javax.crypto.Cipher;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.nio.ByteBuffer;

public class AesCtrCipher {
    private final byte[] key;
    private final byte[] baseNonce;

    public AesCtrCipher(byte[] key, byte[] baseNonce) {
        this.key = key;
        this.baseNonce = baseNonce;
    }

    /**
     * 构建完整的 16 字节 nonce
     * 格式: | 2 bytes base | 2 bytes payload_size | 4 bytes timestamp | 4 bytes timestamp | 4 bytes sequence |
     */
    public byte[] buildNonce(int payloadSize, int timestamp, int sequence) {
        ByteBuffer nonce = ByteBuffer.allocate(16);
        // 前 2 字节: base nonce 前 2 字节
        nonce.put(baseNonce, 0, 2);
        // bytes 2-3: payload size (网络字节序)
        nonce.putShort((short) payloadSize);
        // bytes 4-7: timestamp
        nonce.putInt(timestamp);
        // bytes 8-11: timestamp (重复)
        nonce.putInt(timestamp);
        // bytes 12-15: sequence
        nonce.putInt(sequence);
        return nonce.array();
    }

    /**
     * AES-CTR 解密
     */
    public byte[] decrypt(byte[] encrypted, int payloadSize, int timestamp, int sequence) throws Exception {
        byte[] nonce = buildNonce(payloadSize, timestamp, sequence);

        Cipher cipher = Cipher.getInstance("AES/CTR/NoPadding");
        SecretKeySpec keySpec = new SecretKeySpec(key, "AES");
        IvParameterSpec ivSpec = new IvParameterSpec(nonce);
        cipher.init(Cipher.DECRYPT_MODE, keySpec, ivSpec);

        return cipher.doFinal(encrypted);
    }

    public static byte[] hexStringToBytes(String hex) {
        if (hex == null || hex.length() % 2 != 0) {
            throw new IllegalArgumentException("Invalid hex string");
        }
        byte[] bytes = new byte[hex.length() / 2];
        for (int i = 0; i < bytes.length; i++) {
            bytes[i] = (byte) Integer.parseInt(hex.substring(i * 2, i * 2 + 2), 16);
        }
        return bytes;
    }

    public static String bytesToHexString(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) {
            sb.append(String.format("%02x", b & 0xFF));
        }
        return sb.toString();
    }
}