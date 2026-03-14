# Smart Feeder API Documentation

## Base URL
```
http://localhost:3002
```

## Authentication
All endpoints (except `/health`) require an API key passed in the header:
```
X-API-Key: your-api-key-here
```

---

## Endpoints

### Health Check

**GET** `/health`

Check if the API is running.

**Response:**
```json
{
  "status": "ok",
  "timestamp": "2026-03-13T19:00:00.000Z"
}
```

---

### Feeders

**GET** `/api/feeders`

Get all registered feeders.

**Response:**
```json
{
  "feeders": [
    {
      "id": "feeder_001",
      "name": "Living Room Feeder",
      "foodLevel": 75,
      "isLowFood": false,
      "wifiRssi": -45,
      "uptimeMs": 3600000,
      "lastSeen": "2026-03-13T19:00:00.000Z",
      "createdAt": "2026-03-13T10:00:00.000Z"
    }
  ]
}
```

**POST** `/api/feeders`

Register a new feeder.

**Body:**
```json
{
  "name": "Bedroom Feeder"
}
```

**Response:**
```json
{
  "id": "feeder_abc123",
  "name": "Bedroom Feeder",
  "foodLevel": 100,
  "isLowFood": false,
  "wifiRssi": 0,
  "uptimeMs": 0,
  "lastSeen": "2026-03-13T19:00:00.000Z",
  "createdAt": "2026-03-13T19:00:00.000Z"
}
```

---

### Status Update

**POST** `/api/status`

Update feeder status from device.

**Headers:**
```
X-API-Key: your-api-key-here
Content-Type: application/json
```

**Body:**
```json
{
  "feeder_id": "feeder_001",
  "food_level": 65,
  "is_low_food": false,
  "wifi_rssi": -50,
  "uptime_ms": 7200000
}
```

**Response:**
```json
{
  "success": true
}
```

---

### Trigger Feeding

**POST** `/api/feed`

Trigger a feeding event.

**Body:**
```json
{
  "feeder_id": "feeder_001",
  "type": "manual"
}
```

**Response:**
```json
{
  "success": true,
  "event": {
    "id": "event_123456",
    "feederId": "feeder_001",
    "type": "manual",
    "timestamp": "2026-03-13T19:00:00.000Z",
    "success": true,
    "message": "Feed command sent"
  }
}
```

---

### Schedules

**GET** `/api/schedule`

Get all schedules or filter by feeder.

**Query Parameters:**
- `feeder_id` (optional): Filter by feeder ID

**Response:**
```json
{
  "schedules": [
    {
      "id": "sched_abc123",
      "feederId": "feeder_001",
      "hour": 8,
      "minute": 0,
      "enabled": true,
      "createdAt": "2026-03-13T10:00:00.000Z"
    }
  ]
}
```

**POST** `/api/schedule`

Create a new feeding schedule.

**Body:**
```json
{
  "feeder_id": "feeder_001",
  "hour": 8,
  "minute": 30,
  "enabled": true
}
```

**PUT** `/api/schedule/:id`

Update a schedule.

**Body:**
```json
{
  "hour": 9,
  "minute": 0,
  "enabled": false
}
```

**DELETE** `/api/schedule/:id`

Delete a schedule.

---

### Events

**GET** `/api/events/:feederId`

Get feeding history for a feeder.

**Response:**
```json
{
  "events": [
    {
      "id": "event_123456",
      "feederId": "feeder_001",
      "type": "scheduled",
      "timestamp": "2026-03-13T08:00:00.000Z",
      "success": true
    }
  ]
}
```

---

### Logs

**GET** `/api/logs`

Get API access logs (last 50).

**Response:**
```json
{
  "logs": [
    {
      "id": "log_123456",
      "feederId": "feeder_001",
      "endpoint": "/api/status",
      "method": "POST",
      "statusCode": 200,
      "timestamp": "2026-03-13T19:00:00.000Z"
    }
  ]
}
```

---

## Error Responses

### 401 Unauthorized
```json
{
  "error": "Unauthorized: Invalid API key"
}
```

### 404 Not Found
```json
{
  "error": "Feeder not found"
}
```

### 400 Bad Request
```json
{
  "error": "Missing required fields"
}
```

---

## Rate Limits
Currently no rate limiting is implemented. For production, consider adding rate limiting middleware.

## Webhooks
Not currently supported but can be added for real-time notifications.
