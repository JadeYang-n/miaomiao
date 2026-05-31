package com.antifraud.controller;

import com.antifraud.entity.ChatMessage;
import com.antifraud.repository.ChatMessageRepository;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import java.util.List;

@RestController
@RequestMapping("/api")
public class ChatHistoryController {

    private final ChatMessageRepository repository;

    public ChatHistoryController(ChatMessageRepository repository) {
        this.repository = repository;
    }

    @GetMapping("/history")
    public List<ChatMessage> getHistory(@RequestParam(defaultValue = "100") int memoryId,
                                        @RequestParam(defaultValue = "20") int limit) {
        return repository.findRecentByMemoryId(memoryId, limit);
    }

    @GetMapping("/stats")
    public int getMessageCount(@RequestParam(defaultValue = "100") int memoryId) {
        return repository.countByMemoryId(memoryId);
    }
}
