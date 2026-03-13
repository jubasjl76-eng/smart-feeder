/**
 * Smart Feeder Backend Tests
 * Unit tests for API endpoints
 */

import request from 'supertest';
import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import { app } from './src/index.js';

const API_KEY = 'test-api-key';

describe('Health Endpoint', () => {
  it('should return OK status', async () => {
    const response = await request(app).get('/health');
    expect(response.status).toBe(200);
    expect(response.body.status).toBe('ok');
  });
});

describe('Feeder Endpoints', () => {
  const testFeeder = {
    name: 'Test Feeder'
  };

  it('should create a new feeder', async () => {
    const response = await request(app)
      .post('/api/feeders')
      .set('X-API-Key', API_KEY)
      .send(testFeeder);
    
    expect(response.status).toBe(201);
    expect(response.body).toHaveProperty('id');
    expect(response.body.name).toBe('Test Feeder');
  });

  it('should get all feeders', async () => {
    const response = await request(app)
      .get('/api/feeders')
      .set('X-API-Key', API_KEY);
    
    expect(response.status).toBe(200);
    expect(response.body).toHaveProperty('feeders');
    expect(Array.isArray(response.body.feeders)).toBe(true);
  });

  it('should reject unauthorized requests', async () => {
    const response = await request(app).get('/api/feeders');
    expect(response.status).toBe(401);
  });
});

describe('Schedule Endpoints', () => {
  it('should create a feeding schedule', async () => {
    const schedule = {
      feeder_id: 'feeder_001',
      hour: 8,
      minute: 0,
      enabled: true
    };

    const response = await request(app)
      .post('/api/schedule')
      .set('X-API-Key', API_KEY)
      .send(schedule);
    
    expect(response.status).toBe(201);
    expect(response.body.hour).toBe(8);
    expect(response.body.minute).toBe(0);
  });

  it('should get schedules', async () => {
    const response = await request(app)
      .get('/api/schedule')
      .set('X-API-Key', API_KEY);
    
    expect(response.status).toBe(200);
    expect(response.body).toHaveProperty('schedules');
  });

  it('should validate schedule time', async () => {
    const invalidSchedule = {
      feeder_id: 'feeder_001',
      hour: 25, // Invalid hour
      minute: 0
    };

    const response = await request(app)
      .post('/api/schedule')
      .set('X-API-Key', API_KEY)
      .send(invalidSchedule);
    
    // Backend should validate and reject
    expect(response.status).toBe(400);
  });
});

describe('Status Updates', () => {
  it('should update feeder status', async () => {
    const status = {
      feeder_id: 'feeder_001',
      food_level: 75,
      is_low_food: false,
      wifi_rssi: -45,
      uptime_ms: 3600000
    };

    const response = await request(app)
      .post('/api/status')
      .set('X-API-Key', API_KEY)
      .send(status);
    
    expect(response.status).toBe(200);
    expect(response.body.success).toBe(true);
  });
});

describe('Feeding Events', () => {
  it('should trigger feeding', async () => {
    const feedRequest = {
      feeder_id: 'feeder_001',
      type: 'api'
    };

    const response = await request(app)
      .post('/api/feed')
      .set('X-API-Key', API_KEY)
      .send(feedRequest);
    
    expect(response.status).toBe(200);
    expect(response.body.success).toBe(true);
    expect(response.body.event).toHaveProperty('id');
  });

  it('should get feeding history', async () => {
    const response = await request(app)
      .get('/api/events/feeder_001')
      .set('X-API-Key', API_KEY);
    
    expect(response.status).toBe(200);
    expect(response.body).toHaveProperty('events');
  });
});
