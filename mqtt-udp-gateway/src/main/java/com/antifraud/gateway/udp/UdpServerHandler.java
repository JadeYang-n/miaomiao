package com.antifraud.gateway.udp;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.channel.socket.DatagramPacket;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.antifraud.gateway.session.SessionManager;
import com.antifraud.gateway.audio.AudioForwarder;

import java.net.InetSocketAddress;

/**
 * UDP 包处理
 * 包格式: |type 1u|flags 1u|payload_len 2u|ssrc 4u|timestamp 4u|sequence 4u|payload N|
 */
public class UdpServerHandler extends SimpleChannelInboundHandler<DatagramPacket> {
    private static final Logger log = LoggerFactory.getLogger(UdpServerHandler.class);

    private static final int HEADER_SIZE = 16;
    private static final byte AUDIO_PACKET_TYPE = 0x01;

    private final SessionManager sessionManager;
    private final AudioForwarder audioForwarder;

    public UdpServerHandler(SessionManager sessionManager, AudioForwarder audioForwarder) {
        this.sessionManager = sessionManager;
        this.audioForwarder = audioForwarder;
    }

    @Override
    protected void channelRead0(ChannelHandlerContext ctx, DatagramPacket packet) throws Exception {
        InetSocketAddress remote = packet.sender();
        ByteBuf buf = packet.content();

        if (buf.readableBytes() < HEADER_SIZE) {
            log.warn("Packet too small: {} bytes", buf.readableBytes());
            return;
        }

        // 解析包头
        byte type = buf.readByte();
        byte flags = buf.readByte();
        int payloadLen = buf.readShort() & 0xFFFF;
        int ssrc = buf.readInt();
        int timestamp = buf.readInt();
        int sequence = buf.readInt();

        if (type != AUDIO_PACKET_TYPE) {
            log.debug("Ignoring non-audio packet type: {}", type);
            return;
        }

        // 读取加密的 payload
        byte[] encrypted = new byte[buf.readableBytes()];
        buf.readBytes(encrypted);

        log.debug("UDP packet: seq={}, ts={}, payloadLen={}", sequence, timestamp, payloadLen);

        // 找到对应的 session
        // 这里简化处理，实际需要根据 source IP/port 或 session 查找
        // TODO: 需要维护 remote -> deviceId 的映射
        SessionManager.Session session = findSessionByRemote(remote);
        if (session == null) {
            log.warn("No session found for {}", remote);
            return;
        }

        // 解密
        AesCtrCipher cipher = new AesCtrCipher(session.getAesKey(), session.getBaseNonce());
        byte[] decrypted = cipher.decrypt(encrypted, payloadLen, timestamp, sequence);

        // 转发给 Spring Boot
        audioForwarder.forward(decrypted, session.getDeviceId());
    }

    private SessionManager.Session findSessionByRemote(InetSocketAddress remote) {
        // 简化: 返回第一个 session
        // TODO: 需要维护 remote -> session 映射
        return sessionManager.getSession("default");
    }

    @Override
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) {
        log.error("UDP exception", cause);
    }
}