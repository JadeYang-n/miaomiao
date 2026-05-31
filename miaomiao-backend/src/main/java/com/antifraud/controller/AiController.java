package com.antifraud.controller;

import com.antifraud.entity.UserProfile;
import com.antifraud.repository.UserProfileRepository;
import com.antifraud.service.AiService;
import jakarta.annotation.Resource;
import org.springframework.http.codec.ServerSentEvent;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import reactor.core.publisher.Flux;

import java.util.Optional;

@RestController
@RequestMapping("/ai")
public class AiController {

    @Resource
    private AiService aiService;

    @Resource
    private UserProfileRepository userProfileRepository;

    @GetMapping("/chat")
    public Flux<ServerSentEvent<String>> chat(int memoryId, String message) {
        return aiService.chatStream(memoryId, message)
                .map(chunk -> ServerSentEvent.<String>builder()
                        .data(chunk)
                        .build());
    }

    @GetMapping("/profile")
    public Object getProfile(int id) {
        Optional<UserProfile> profile = userProfileRepository.findById(id);
        if (profile.isPresent()) {
            return profile.get();
        }
        return java.util.Map.of("error", "not found", "id", id);
    }

    @GetMapping("/profile/test")
    public Object testProfileExtraction(String message) {
        if (message == null || message.isEmpty()) {
            return java.util.Map.of("error", "message is required");
        }
        String result = aiService.chatSyncForChild(message);
        return java.util.Map.of("ai_response", result, "profile", userProfileRepository.findById(1).orElse(null));
    }

    @GetMapping("/profile/clear")
    public Object clearProfile() {
        userProfileRepository.deleteById(1);
        return java.util.Map.of("status", "cleared");
    }

}