package com.antifraud.gateway;

import com.antifraud.gateway.config.AppConfig;
import com.antifraud.gateway.mqtt.SimpleMqttBroker;
import com.antifraud.gateway.udp.UdpServer;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import com.antifraud.gateway.session.SessionManager;

@SpringBootApplication
public class GatewayApplication {

    public static void main(String[] args) {
        SpringApplication.run(GatewayApplication.class, args);
    }

    @Bean
    public SessionManager sessionManager() {
        return new SessionManager();
    }

    @Bean
    public ObjectMapper objectMapper() {
        return new ObjectMapper();
    }

    @Bean
    public SimpleMqttBroker mqttBroker(SessionManager sessionManager, ObjectMapper objectMapper) {
        return new SimpleMqttBroker(1883, sessionManager, objectMapper);
    }

    @Bean
    public UdpServer udpServer(AppConfig config, SessionManager sessionManager, com.antifraud.gateway.audio.AudioForwarder audioForwarder) {
        return new UdpServer(config, sessionManager, audioForwarder);
    }

    @Bean
    public com.antifraud.gateway.audio.AudioForwarder audioForwarder(AppConfig config, ObjectMapper objectMapper) {
        return new com.antifraud.gateway.audio.AudioForwarder(config, objectMapper);
    }
}