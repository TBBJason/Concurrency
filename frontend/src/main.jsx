import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import './index.css'
import WebSocketChat from './WebSocketChat.jsx'

createRoot(document.getElementById('root')).render(
  <StrictMode>
    <WebSocketChat />
  </StrictMode>,
)
