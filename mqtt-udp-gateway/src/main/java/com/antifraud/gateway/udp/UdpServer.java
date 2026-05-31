package com.antifraud.gateway.udp;

import com.antifraud.gateway.config.AppConfig;
import com.antifraud.gateway.session.SessionManager;
import com.antifraud.gateway.audio.AudioForwarder;
import io.netty.bootstrap.Bootstrap;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.nio.NioDatagramChannel;
import io.netty.channel.socket.DatagramPacket;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import org.springframework.stereotype.Component;

@Component
public class UdpServer {
    private static final Logger log = LoggerFactory.getLogger(UdpServer.class);

    private final AppConfig config;
    private final SessionManager sessionManager;
    private final AudioForwarder audioForwarder;
    private NioEventLoopGroup group;
    private volatile boolean running = false;

    public UdpServer(AppConfig config, SessionManager sessionManager, AudioForwarder audioForwarder) {
        this.config = config;
        this.sessionManager = sessionManager;
        this.audioForwarder = audioForwarder;
    }

    @PostConstruct
    public void start() {
        if (running) return;
        running = true;

        group = new NioEventLoopGroup();
        int port = config.getUdp().getPort();

        Bootstrap bootstrap = new Bootstrap();
        bootstrap.group(group)
                .channel(NioDatagramChannel.class)
                .handler(new ChannelInitializer<NioDatagramChannel>() {
                    @Override
                    protected void initChannel(NioDatagramChannel ch) {
                        ch.pipeline().addLast(new UdpServerHandler(sessionManager, audioForwarder));
                    }
                });

        try {
            bootstrap.bind(port).sync();
            log.info("UDP server started on port {}", port);
        } catch (Exception e) {
            log.error("Failed to start UDP server", e);
        }
    }

    @PreDestroy
    public void stop() {
        running = false;
        if (group != null) {
            group.shutdownGracefully();
        }
    }
}