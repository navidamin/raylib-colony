# HORIZON 002 | 5-Dec-25
## Planned Features & Progression

---

## COMPLETED PHASES

### Phase 0: Foundation ✅ 100%
- Engine refactor (managers)
- View system
- Data-driven types (game_types.toml)
- Multiple active modules
- Texture rendering
- Typed resources (4 categories, 14 subtypes)
- Constant MANPOWER
- Ambient solar energy

### Phase 1: Core Resource Flow ✅ Core Complete
- Storage capacity system
- Sect↔Colony push/pull flow
- Graceful degradation (efficiency scaling)
- *Skipped: Priority allocation, request queue*

---

## NEXT PHASES

### Phase 2: Transport Network
**Status:** Next up

#### 2.1 Road System
- [ ] Road data structure (Sect A ↔ Sect B)
- [ ] Road construction with progress
- [ ] Distance calculation between sects

#### 2.2 Transport Jobs
- [ ] TransportJob struct (source, dest, resource, amount)
- [ ] Job queue per colony
- [ ] Basic pathfinding (direct routes)

#### 2.3 Visual Transport
- [ ] Animated resource packets on roads
- [ ] Color-coded by resource type
- [ ] Progress indicators

#### 2.4 Transport Timing
- [ ] Distance-based travel time
- [ ] Transport module speed bonuses

---

### Phase 3: Production Intelligence
**Status:** Planned

#### 3.1 Strategy Pattern
- [ ] ProductionStrategy base class
- [ ] Unit-specific strategies
- [ ] Clean separation of logic

#### 3.2 Smart Distribution
- [ ] Resource priority learning
- [ ] Adaptive allocation
- [ ] Player action tracking

#### 3.3 Module Chaining
- [ ] Internal buffers
- [ ] Output→Input chains
- [ ] Vertical integration

---

### Phase 4: Research & Technology
**Status:** Planned

- [ ] ResearchManager class
- [ ] SCIENCE accumulation
- [ ] Tech tree (TOML-defined)
- [ ] Unlock effects (modules, bonuses)
- [ ] Research UI

---

### Phase 5: Polish & Optimization
**Status:** Future

- [ ] Performance profiling
- [ ] UI/UX improvements
- [ ] Particle effects
- [ ] Sound/music
- [ ] Tutorial

---

## DEFERRED FEATURES

*Moved from Phase 1 for later:*
- Priority-based resource allocation
- Request/fulfillment queue system
- Population growth/decline mechanics

---

## DEPENDENCIES

```
Phase 0 ✅ → Phase 1 ✅ → Phase 2 (transport)
                              ↓
                         Phase 3 (intelligence)
                              ↓
                         Phase 4 (research)
                              ↓
                         Phase 5 (polish)
```

---

## IMMEDIATE NEXT STEPS

1. **Road Data Structure**
   - Add `roads` vector to Colony (already exists as pairs)
   - Add road length/travel time calculation

2. **Transport Job**
   - Create TransportJob struct
   - Add job queue to Colony
   - Process jobs in Update()

3. **Visual Feedback**
   - Draw resource packets moving along roads
   - Show transport progress

---

## TECHNICAL DEBT

| Priority | Item |
|----------|------|
| Low | Remove debug cout statements |
| Low | Add proper logging system |
| Medium | Save/load system |
| Medium | Unit testing framework |

---

**Next Review:** After Phase 2 transport basics
