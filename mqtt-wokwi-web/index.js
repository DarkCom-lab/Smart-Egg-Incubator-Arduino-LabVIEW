const express = require('express');
const mqtt = require('mqtt');
const http = require('http');
const socketIo = require('socket.io');
const cors = require('cors');
const path = require('path');

// Configuration
const MQTT_BROKER = 'mqtt://broker.emqx.io';
const MQTT_PORT = 1883;
const WEB_PORT = 3000;

// MQTT Topics
const TOPICS = {
  HEARTBEAT: 'incubator/heartbeat',
  SENSOR_READINGS: 'incubator/sensor_readings',
  CONTROL_SETPOINTS: 'incubator/control_setpoints',
  MANUAL_CONTROLS: 'incubator/manual_controls',
  SYSTEM_STATUS: 'incubator/system_status',
  COMMANDS: 'incubator/commands'
};

// Initialize Express app
const app = express();
const server = http.createServer(app);
const io = socketIo(server, {
  cors: {
    origin: "*",
    methods: ["GET", "POST"]
  }
});

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static('public'));

// Store latest sensor data
let sensorData = {
  // Sensor Readings
  temperature: 25.0,
  humidity: 60.0,
  potentiometer: 0,
  servoPosition: 0,
  
  // Control Setpoints
  maxTemperature: 30,
  maxHumidity: 80,
  mode: "AUTO",
  
  // Manual Controls
  fanRunning: false,
  windowOpen: false,
  ledStatus: false,
  buzzerStatus: false,
  
  // System Status
  sensorStatus: true,
  wifi_rssi: -70,
  free_memory: 0,
  client_id: "unknown",
  
  // Meta data
  timestamp: Date.now(),
  esp32_connected: false,
  last_seen: 0,
  send_counter: 0
};

// Track ESP32 connection status
let esp32LastSeen = 0;
const ESP32_TIMEOUT = 10000;
let mqttMessageCount = 0;
let websocketMessageCount = 0;

// Connect to MQTT broker
console.log(`🔌 Connecting to MQTT broker: ${MQTT_BROKER}`);
const mqttClient = mqtt.connect(MQTT_BROKER, {
  clientId: 'nodejs_server_' + Math.random().toString(16).substr(2, 8),
  clean: true,
  connectTimeout: 4000,
  reconnectPeriod: 1000
});

// MQTT connection handlers
mqttClient.on('connect', () => {
  console.log('✅ Connected to MQTT broker');
  
  // Subscribe to all topics
  const topics = Object.values(TOPICS);
  mqttClient.subscribe(topics, (err) => {
    if (err) {
      console.error('❌ Subscription error:', err);
    } else {
      console.log(`📡 Subscribed to ${topics.length} topics:`);
      topics.forEach(topic => console.log(`   - ${topic}`));
    }
  });
});

mqttClient.on('message', (topic, message) => {
  mqttMessageCount++;
  const now = Date.now();
  const timestamp = new Date().toISOString().split('T')[1].split('.')[0];
  
  try {
    const data = JSON.parse(message.toString());
    
    console.log(`\n📨 [${timestamp}] MQTT #${mqttMessageCount} on "${topic}":`);
    
    // Update ESP32 connection status
    esp32LastSeen = now;
    sensorData.esp32_connected = true;
    sensorData.last_seen = now;
    
    // Merge data based on topic
    switch(topic) {
      case TOPICS.HEARTBEAT:
        console.log(`   💓 Heartbeat from ${data.client_id || 'ESP32'}`);
        console.log(`      RSSI: ${data.rssi || 'N/A'}dBm, Uptime: ${Math.floor((data.uptime || 0)/1000)}s`);
        sensorData.wifi_rssi = data.rssi || -70;
        sensorData.client_id = data.client_id || 'esp32_incubator';
        
        // Broadcast heartbeat update
        io.emit('heartbeat-update', {
          client_id: data.client_id,
          rssi: data.rssi,
          uptime: data.uptime,
          timestamp: now
        });
        break;
        
      case TOPICS.SENSOR_READINGS:
        console.log(`   📊 Sensor Readings:`);
        console.log(`      Temp: ${data.temperature}°C, Hum: ${data.humidity}%`);
        console.log(`      Pot: ${data.potentiometer}, Servo: ${data.servoPosition}°`);
        
        sensorData = { 
          ...sensorData, 
          temperature: data.temperature || sensorData.temperature,
          humidity: data.humidity || sensorData.humidity,
          potentiometer: data.potentiometer || sensorData.potentiometer,
          servoPosition: data.servoPosition || sensorData.servoPosition,
          send_counter: data.send_counter || sensorData.send_counter
        };
        
        // Broadcast sensor readings update
        io.emit('sensor-readings-update', {
          temperature: data.temperature,
          humidity: data.humidity,
          potentiometer: data.potentiometer,
          servoPosition: data.servoPosition,
          timestamp: now
        });
        break;
        
      case TOPICS.CONTROL_SETPOINTS:
        console.log(`   🎯 Control Setpoints:`);
        console.log(`      Max Temp: ${data.maxTemperature}°C, Max Hum: ${data.maxHumidity}%`);
        console.log(`      Mode: ${data.mode || 'AUTO'}`);
        
        sensorData = { 
          ...sensorData, 
          maxTemperature: data.maxTemperature || sensorData.maxTemperature,
          maxHumidity: data.maxHumidity || sensorData.maxHumidity,
          mode: data.mode || sensorData.mode,
          send_counter: data.send_counter || sensorData.send_counter
        };
        
        // Broadcast control setpoints update
        io.emit('control-setpoints-update', {
          maxTemperature: data.maxTemperature,
          maxHumidity: data.maxHumidity,
          mode: data.mode,
          timestamp: now
        });
        break;
        
      case TOPICS.MANUAL_CONTROLS:
        console.log(`   🎮 Manual Controls:`);
        console.log(`      Fan: ${data.fanRunning ? 'ON' : 'OFF'}, Window: ${data.windowOpen ? 'OPEN' : 'CLOSED'}`);
        console.log(`      LED: ${data.ledStatus ? 'ON' : 'OFF'}, Buzzer: ${data.buzzerStatus ? 'ON' : 'OFF'}`);
        
        sensorData = { 
          ...sensorData, 
          fanRunning: data.fanRunning || false,
          windowOpen: data.windowOpen || false,
          ledStatus: data.ledStatus || false,
          buzzerStatus: data.buzzerStatus || false,
          send_counter: data.send_counter || sensorData.send_counter
        };
        
        // Broadcast manual controls update
        io.emit('manual-controls-update', {
          fanRunning: data.fanRunning,
          windowOpen: data.windowOpen,
          ledStatus: data.ledStatus,
          buzzerStatus: data.buzzerStatus,
          timestamp: now
        });
        break;
        
      case TOPICS.SYSTEM_STATUS:
        console.log(`   📈 System Status:`);
        console.log(`      Sensor: ${data.sensorStatus ? 'OK' : 'ERROR'}, RSSI: ${data.wifi_rssi || 'N/A'}dBm`);
        console.log(`      Memory: ${data.free_memory || 'N/A'} bytes`);
        
        sensorData = { 
          ...sensorData, 
          sensorStatus: data.sensorStatus !== undefined ? data.sensorStatus : sensorData.sensorStatus,
          wifi_rssi: data.wifi_rssi || sensorData.wifi_rssi,
          free_memory: data.free_memory || sensorData.free_memory,
          client_id: data.client_id || sensorData.client_id,
          send_counter: data.send_counter || sensorData.send_counter
        };
        
        // Broadcast system status update
        io.emit('system-status-update', {
          sensorStatus: data.sensorStatus,
          wifi_rssi: data.wifi_rssi,
          free_memory: data.free_memory,
          client_id: data.client_id,
          timestamp: now
        });
        break;
        
      default:
        console.log(`   ℹ️ Unknown topic: ${message.toString()}`);
    }
    
    // Send full update to new clients
    websocketMessageCount++;
    io.emit('full-update', sensorData);
    console.log(`   📤 Sent to WebSocket (total: ${websocketMessageCount})`);
    
  } catch (error) {
    console.error('❌ Error parsing MQTT message:', error);
    console.log('Raw message:', message.toString());
  }
});

mqttClient.on('error', (err) => {
  console.error('❌ MQTT Error:', err);
});

mqttClient.on('offline', () => {
  console.log('⚠️ MQTT client offline');
});

// Function to request all data from ESP32
function requestAllData() {
  if (mqttClient.connected) {
    const command = { command: "GET_ALL_DATA" };
    console.log('📡 Sending GET_ALL_DATA command to ESP32');
    mqttClient.publish(TOPICS.COMMANDS, JSON.stringify(command), { qos: 1 }, (err) => {
      if (err) {
        console.error('❌ Failed to send command:', err);
      } else {
        console.log('✅ Command sent to ESP32');
      }
    });
  } else {
    console.log('⚠️ Cannot send command: MQTT not connected');
  }
}

// Function to check ESP32 connection status
function checkESP32Connection() {
  const now = Date.now();
  const timeSinceLastSeen = now - esp32LastSeen;
  
  if (esp32LastSeen > 0 && timeSinceLastSeen > ESP32_TIMEOUT) {
    if (sensorData.esp32_connected) {
      console.log(`⚠️ ESP32 connection lost - last seen ${Math.floor(timeSinceLastSeen/1000)} seconds ago`);
      sensorData.esp32_connected = false;
      
      // Broadcast connection lost
      io.emit('connection-lost', {
        last_seen: esp32LastSeen,
        time_since_last_seen: timeSinceLastSeen
      });
    }
  }
}

// Periodic status check and sync
setInterval(() => {
  checkESP32Connection();
  
  // If ESP32 is connected, request all data every 30 seconds
  if (sensorData.esp32_connected) {
    const timeSinceUpdate = Date.now() - sensorData.timestamp;
    if (timeSinceUpdate > 30000) {
      console.log('🔄 Auto-refreshing ESP32 data...');
      requestAllData();
    }
  }
}, 5000);

// WebSocket connection handler
io.on('connection', (socket) => {
  const clientId = socket.id.substring(0, 6);
  console.log(`🔌 New client connected: ${clientId}`);
  
  // Send current full data to new client
  socket.emit('full-update', sensorData);
  
  // Send individual topic updates
  socket.emit('sensor-readings-update', {
    temperature: sensorData.temperature,
    humidity: sensorData.humidity,
    potentiometer: sensorData.potentiometer,
    servoPosition: sensorData.servoPosition
  });
  
  socket.emit('control-setpoints-update', {
    maxTemperature: sensorData.maxTemperature,
    maxHumidity: sensorData.maxHumidity,
    mode: sensorData.mode
  });
  
  socket.emit('manual-controls-update', {
    fanRunning: sensorData.fanRunning,
    windowOpen: sensorData.windowOpen,
    ledStatus: sensorData.ledStatus,
    buzzerStatus: sensorData.buzzerStatus
  });
  
  socket.emit('system-status-update', {
    sensorStatus: sensorData.sensorStatus,
    wifi_rssi: sensorData.wifi_rssi,
    free_memory: sensorData.free_memory,
    client_id: sensorData.client_id
  });
  
  // Handle control commands from client
  socket.on('control-command', (command) => {
    console.log(`🎮 Command from ${clientId}:`, command);
    
    // Publish command to MQTT
    mqttClient.publish(TOPICS.COMMANDS, JSON.stringify(command), { qos: 1 }, (err) => {
      if (err) {
        console.error('❌ Publish error:', err);
        socket.emit('command-error', { 
          message: 'Failed to send command',
          command: command 
        });
      } else {
        console.log(`✅ Command sent to ESP32:`, command);
        socket.emit('command-success', { 
          message: 'Command sent successfully',
          command: command 
        });
        
        // Request updated data after command (with delay)
        setTimeout(() => {
          requestAllData();
        }, 1000);
      }
    });
  });
  
  // Handle status request from client
  socket.on('request-all-data', () => {
    console.log(`🔄 ${clientId} requested all data`);
    requestAllData();
    socket.emit('command-success', {
      message: 'Data request sent to ESP32'
    });
  });
  
  // Handle manual sync request
  socket.on('force-sync', () => {
    console.log(`⚡ ${clientId} requested force sync`);
    requestAllData();
    
    // Also send current data immediately
    socket.emit('full-update', sensorData);
    socket.emit('command-success', {
      message: 'Force sync completed'
    });
  });
  
  // Handle debug request
  socket.on('get-status', () => {
    console.log(`🔧 ${clientId} requested status`);
    const status = {
      server_time: new Date().toISOString(),
      uptime: process.uptime(),
      mqtt_connected: mqttClient.connected,
      mqtt_messages: mqttMessageCount,
      websocket_messages: websocketMessageCount,
      connected_clients: io.engine.clientsCount,
      esp32_connected: sensorData.esp32_connected,
      last_seen: sensorData.last_seen,
      sensor_data: sensorData
    };
    socket.emit('status-response', status);
  });
  
  socket.on('disconnect', () => {
    console.log(`🔌 Client disconnected: ${clientId}`);
  });
});

// API Routes
app.get('/api/status', (req, res) => {
  const now = Date.now();
  const timeSinceLastSeen = now - esp32LastSeen;
  
  res.json({
    status: 'online',
    server_time: new Date().toISOString(),
    uptime: process.uptime(),
    
    mqtt: {
      connected: mqttClient.connected,
      broker: MQTT_BROKER,
      messages_received: mqttMessageCount,
      topics: Object.values(TOPICS)
    },
    
    websocket: {
      messages_sent: websocketMessageCount,
      clients_connected: io.engine.clientsCount
    },
    
    esp32: {
      connected: sensorData.esp32_connected,
      last_seen: sensorData.last_seen,
      time_since_last_seen: timeSinceLastSeen,
      client_id: sensorData.client_id,
      wifi_rssi: sensorData.wifi_rssi
    },
    
    sensor_data: sensorData
  });
});

app.post('/api/control', (req, res) => {
  const command = req.body;
  
  if (!command || !command.command) {
    return res.status(400).json({ error: 'Invalid command' });
  }
  
  mqttClient.publish(TOPICS.COMMANDS, JSON.stringify(command), { qos: 1 }, (err) => {
    if (err) {
      res.status(500).json({ error: 'Failed to send command' });
    } else {
      // Request updated data after command
      setTimeout(requestAllData, 1000);
      res.json({ success: true, command: command });
    }
  });
});

// API endpoint to force sync with ESP32
app.post('/api/sync', (req, res) => {
  requestAllData();
  res.json({ 
    success: true, 
    message: 'Sync request sent to ESP32',
    timestamp: Date.now()
  });
});

// API endpoint to get MQTT topics info
app.get('/api/topics', (req, res) => {
  res.json({
    topics: TOPICS,
    description: {
      heartbeat: 'ESP32 connection status and basic info',
      sensor_readings: 'Temperature, humidity, potentiometer, servo position',
      control_setpoints: 'Max temperature, max humidity, control mode',
      manual_controls: 'Fan, window, LED, buzzer states',
      system_status: 'System health, WiFi signal, memory usage',
      commands: 'Commands sent from server to ESP32'
    }
  });
});

// Health check endpoint
app.get('/health', (req, res) => {
  res.json({
    status: 'healthy',
    uptime: process.uptime(),
    mqttConnected: mqttClient.connected,
    esp32Connected: sensorData.esp32_connected,
    clientsConnected: io.engine.clientsCount,
    timestamp: new Date().toISOString()
  });
});

// Serve the main page
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Start server
server.listen(WEB_PORT, () => {
  console.log(`\n🚀 Server running on http://localhost:${WEB_PORT}`);
  console.log(`📡 MQTT Broker: ${MQTT_BROKER}`);
  console.log(`📊 Topics configured: ${Object.keys(TOPICS).length}`);
  console.log(`🔧 Auto-refresh enabled (30s interval)`);
  console.log(`\n=== Waiting for ESP32 data ===\n`);
  
  // Request initial data from ESP32
  setTimeout(requestAllData, 2000);
});
