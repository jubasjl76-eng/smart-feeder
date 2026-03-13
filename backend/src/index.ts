/**
 * Smart Feeder Backend API
 * Node.js + Express + TypeScript
 * 
 * Features:
 * - API key authentication
 * - Multiple feeder management
 * - Feeding schedules
 * - Event logging
 * - RESTful endpoints
 */

import express, { Express, Request, Response, NextFunction } from 'express';
import cors from 'cors';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import crypto from 'crypto';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app: Express = express();
const PORT = 3002;
const DATA_DIR = path.join(__dirname, 'data');

// ============== TYPES ==============
interface Feeder {
  id: string;
  name: string;
  foodLevel: number;
  isLowFood: boolean;
  wifiRssi: number;
  uptimeMs: number;
  lastSeen: string;
  createdAt: string;
}

interface Schedule {
  id: string;
  feederId: string;
  hour: number;
  minute: number;
  enabled: boolean;
  createdAt: string;
}

interface FeedEvent {
  id: string;
  feederId: string;
  type: 'scheduled' | 'manual' | 'api';
  timestamp: string;
  success: boolean;
  message?: string;
}

interface ApiLog {
  id: string;
  feederId: string;
  endpoint: string;
  method: string;
  statusCode: number;
  timestamp: string;
}

// ============== DATA STORAGE ==============
interface Database {
  feeders: Feeder[];
  schedules: Schedule[];
  events: FeedEvent[];
  logs: ApiLog[];
}

let db: Database = {
  feeders: [],
  schedules: [],
  events: [],
  logs: []
};

const DB_FILE = path.join(DATA_DIR, 'database.json');

// Ensure data directory exists
if (!fs.existsSync(DATA_DIR)) {
  fs.mkdirSync(DATA_DIR, { recursive: true });
}

// Load database
if (fs.existsSync(DB_FILE)) {
  try {
    const data = fs.readFileSync(DB_FILE, 'utf-8');
    db = JSON.parse(data);
  } catch (err) {
    console.error('Failed to load database:', err);
  }
}

// Save database
const saveDb = (): void => {
  fs.writeFileSync(DB_FILE, JSON.stringify(db, null, 2));
};

// ============== MIDDLEWARE ==============
app.use(cors());
app.use(express.json());

// API Key authentication
const API_KEY = process.env.API_KEY || 'your-api-key-here';

const authMiddleware = (req: Request, res: Response, next: NextFunction): void => {
  const providedKey = req.headers['x-api-key'] as string;
  
  if (!providedKey || providedKey !== API_KEY) {
    res.status(401).json({ error: 'Unauthorized: Invalid API key' });
    return;
  }
  
  next();
};

// ============== HEALTH CHECK ==============
app.get('/health', (_req: Request, res: Response) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// ============== FEEDER ENDPOINTS ==============

// Get all feeders
app.get('/api/feeders', authMiddleware, (_req: Request, res: Response) => {
  res.json({ feeders: db.feeders });
});

// Get single feeder
app.get('/api/feeders/:id', authMiddleware, (req: Request, res: Response) => {
  const feeder = db.feeders.find(f => f.id === req.params.id);
  
  if (!feeder) {
    res.status(404).json({ error: 'Feeder not found' });
    return;
  }
  
  res.json(feeder);
});

// Register new feeder
app.post('/api/feeders', authMiddleware, (req: Request, res: Response) => {
  const { name } = req.body;
  
  const feeder: Feeder = {
    id: `feeder_${crypto.randomBytes(4).toString('hex')}`,
    name: name || 'Smart Feeder',
    foodLevel: 100,
    isLowFood: false,
    wifiRssi: 0,
    uptimeMs: 0,
    lastSeen: new Date().toISOString(),
    createdAt: new Date().toISOString()
  };
  
  db.feeders.push(feeder);
  saveDb();
  
  res.status(201).json(feeder);
});

// Update feeder status
app.post('/api/status', authMiddleware, (req: Request, res: Response) => {
  const { feeder_id, food_level, is_low_food, wifi_rssi, uptime_ms } = req.body;
  
  let feeder = db.feeders.find(f => f.id === feeder_id);
  
  if (!feeder) {
    // Auto-register if not exists
    feeder = {
      id: feeder_id,
      name: 'Smart Feeder',
      foodLevel: food_level,
      isLowFood: is_low_food,
      wifiRssi: wifi_rssi,
      uptimeMs: uptime_ms,
      lastSeen: new Date().toISOString(),
      createdAt: new Date().toISOString()
    };
    db.feeders.push(feeder);
  } else {
    feeder.foodLevel = food_level;
    feeder.isLowFood = is_low_food;
    feeder.wifiRssi = wifi_rssi;
    feeder.uptimeMs = uptime_ms;
    feeder.lastSeen = new Date().toISOString();
  }
  
  // Log the API call
  const logEntry: ApiLog = {
    id: `log_${Date.now()}`,
    feederId: feeder_id,
    endpoint: '/api/status',
    method: 'POST',
    statusCode: 200,
    timestamp: new Date().toISOString()
  };
  db.logs.push(logEntry);
  
  // Keep only last 100 logs
  if (db.logs.length > 100) {
    db.logs = db.logs.slice(-100);
  }
  
  saveDb();
  res.json({ success: true });
});

// ============== FEEDING ENDPOINTS ==============

// Trigger feeding
app.post('/api/feed', authMiddleware, (req: Request, res: Response) => {
  const { feeder_id, type = 'api' } = req.body;
  
  const feeder = db.feeders.find(f => f.id === feeder_id);
  
  if (!feeder) {
    res.status(404).json({ error: 'Feeder not found' });
    return;
  }
  
  const event: FeedEvent = {
    id: `event_${Date.now()}`,
    feederId: feeder_id,
    type: type as 'scheduled' | 'manual' | 'api',
    timestamp: new Date().toISOString(),
    success: true,
    message: 'Feed command sent'
  };
  
  db.events.push(event);
  
  // Keep only last 100 events
  if (db.events.length > 100) {
    db.events = db.events.slice(-100);
  }
  
  saveDb();
  res.json({ success: true, event });
});

// Get feeding history
app.get('/api/events/:feederId', authMiddleware, (req: Request, res: Response) => {
  const events = db.events.filter(e => e.feederId === req.params.feederId);
  res.json({ events });
});

// ============== SCHEDULE ENDPOINTS ==============

// Get schedules for feeder
app.get('/api/schedule', authMiddleware, (req: Request, res: Response) => {
  const feederId = req.query.feeder_id as string;
  
  let schedules = db.schedules;
  if (feederId) {
    schedules = db.schedules.filter(s => s.feederId === feederId);
  }
  
  res.json({ schedules });
});

// Create schedule
app.post('/api/schedule', authMiddleware, (req: Request, res: Response) => {
  const { feeder_id, hour, minute, enabled = true } = req.body;
  
  if (!feeder_id || hour === undefined || minute === undefined) {
    res.status(400).json({ error: 'Missing required fields' });
    return;
  }
  
  const schedule: Schedule = {
    id: `sched_${crypto.randomBytes(4).toString('hex')}`,
    feederId: feeder_id,
    hour,
    minute,
    enabled,
    createdAt: new Date().toISOString()
  };
  
  db.schedules.push(schedule);
  saveDb();
  
  res.status(201).json(schedule);
});

// Update schedule
app.put('/api/schedule/:id', authMiddleware, (req: Request, res: Response) => {
  const { hour, minute, enabled } = req.body;
  
  const schedule = db.schedules.find(s => s.id === req.params.id);
  
  if (!schedule) {
    res.status(404).json({ error: 'Schedule not found' });
    return;
  }
  
  if (hour !== undefined) schedule.hour = hour;
  if (minute !== undefined) schedule.minute = minute;
  if (enabled !== undefined) schedule.enabled = enabled;
  
  saveDb();
  res.json(schedule);
});

// Delete schedule
app.delete('/api/schedule/:id', authMiddleware, (req: Request, res: Response) => {
  const index = db.schedules.findIndex(s => s.id === req.params.id);
  
  if (index === -1) {
    res.status(404).json({ error: 'Schedule not found' });
    return;
  }
  
  db.schedules.splice(index, 1);
  saveDb();
  
  res.json({ success: true });
});

// ============== LOGGING ENDPOINTS ==============

// Get API logs
app.get('/api/logs', authMiddleware, (_req: Request, res: Response) => {
  res.json({ logs: db.logs.slice(-50) });
});

// ============== ERROR HANDLING ==============
app.use((_req: Request, res: Response) => {
  res.status(404).json({ error: 'Not found' });
});

app.use((err: Error, _req: Request, res: Response, _next: NextFunction) => {
  console.error(err.stack);
  res.status(500).json({ error: 'Internal server error' });
});

// ============== START SERVER ==============
app.listen(PORT, () => {
  console.log(`Smart Feeder API running on http://localhost:${PORT}`);
  console.log(`API Key: ${API_KEY}`);
  console.log('');
  console.log('Endpoints:');
  console.log('  GET  /health              - Health check');
  console.log('  GET  /api/feeders          - List all feeders');
  console.log('  POST /api/feeders          - Register new feeder');
  console.log('  POST /api/status           - Update feeder status');
  console.log('  POST /api/feed             - Trigger feeding');
  console.log('  GET  /api/schedule         - Get schedules');
  console.log('  POST /api/schedule         - Create schedule');
  console.log('  PUT  /api/schedule/:id     - Update schedule');
  console.log('  DELETE /api/schedule/:id  - Delete schedule');
  console.log('');
  console.log('Headers required: X-API-Key: your-api-key-here');
});
