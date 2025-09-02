import React, { useState, useEffect, useRef } from 'react';
import './WebSocketChat.css';

const WebSocketChat = () => {
  const [messages, setMessages] = useState([]);
  const [inputValue, setInputValue] = useState('');
  const [isConnected, setIsConnected] = useState(false);
  const ws = useRef(null);
  const chatEndRef = useRef(null);

  useEffect(() => {
    ws.current = new WebSocket('ws://localhost:8080');

    ws.current.onopen = () => {
      console.log('Connected to WebSocket server');
      setIsConnected(true);
    };

    ws.current.onmessage = (event) => {
      setMessages((prev) => [...prev, { text: event.data, isOwn: false }]);
    };

    ws.current.onclose = () => {
      console.log('Disconnected from WebSocket server');
      setIsConnected(false);
    };

    return () => {
      if (ws.current) {
        ws.current.close();
      }
    };
  }, []);

  // Auto-scroll when messages change
  useEffect(() => {
    if (chatEndRef.current) {
      chatEndRef.current.scrollIntoView({ behavior: 'smooth', block: 'end' });
    }
  }, [messages]);

  const sendMessage = () => {
    const trimmed = inputValue.trim();
    if (trimmed && ws.current && isConnected) {
      ws.current.send(trimmed);
      setMessages((prev) => [...prev, { text: trimmed, isOwn: true }]);
      setInputValue('');
    }
  };

  const handleKeyDown = (e) => {
    if (e.key === 'Enter') {
      sendMessage();
    }
  };

  return (
    <div className="ws-container">
      <h2 className="ws-title">WebSocket Chat</h2>

      <div className="ws-status">
        Status:{' '}
        {isConnected ? (
          <span className="ws-connected">Connected</span>
        ) : (
          <span className="ws-disconnected">Disconnected</span>
        )}
      </div>

      <div className="ws-chat" role="log" aria-live="polite">
        {messages.map((message, index) => (
          <div
            key={index}
            className={`ws-message ${message.isOwn ? 'ws-own' : 'ws-other'}`}
          >
            {message.text}
          </div>
        ))}
        <div ref={chatEndRef} />
      </div>

      <div className="ws-input-container">
        <input
          className="ws-input"
          type="text"
          value={inputValue}
          onChange={(e) => setInputValue(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder="Type your message..."
          disabled={!isConnected}
          aria-label="Message input"
        />
        <button
          className="ws-button"
          onClick={sendMessage}
          disabled={!isConnected}
          aria-label="Send message"
        >
          Send
        </button>
      </div>
    </div>
  );
};

export default WebSocketChat;
