# HORIZON 003 | 5-Dec-25
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

### Phase 2: Transport Network ✅ Core Complete
- Road data structure with distance/travel time
- TransportJob system with progress tracking
- Three transport modes (Auto/Manual/Deficit-triggered)
- Visual roads with dashed lines (color-coded by mode)
- Animated transport packets on roads

### Phase 2.5: Transport Polish ✅ Complete (2025-12-19)
- Road construction UI with two-click build mode (KEY_B)
- Road selection system (KEY_2 selects nearest road)
- Visual feedback for selected road (highlight + info panel)
- Mode cycling on selected road (KEY_T)
- Rate limiting (MIN_TRANSPORT_INTERVAL = 3s)
- Multiple packets per road (MAX_PACKETS_PER_ROAD = 3)
- Test infrastructure (KEY_0,1,2,3 for debugging)
- Screenshot capability (F12)

---

## NEXT PHASES

### Phase 2.5: Transport Polish ✅ COMPLETE
~~**Status:** Next up~~

#### 2.5.1 Road Construction UI ✅
- [x] Road building mode (select two sects) - KEY_B toggle
- [x] Visual feedback during construction
- [ ] Construction time/cost (deferred)

#### 2.5.2 Transport Mode UI ✅
- [x] Click road to see/change mode - KEY_2 + KEY_T
- [x] Mode indicator on roads - color-coded
- [x] Info panel for selected road

#### 2.5.3 Transport Balancing ✅
- [x] Rate limiting for transport jobs - 3 second interval
- [x] Multiple packets on same road - max 3

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

*Moved from earlier phases:*
- Priority-based resource allocation
- Request/fulfillment queue system
- Population growth/decline mechanics

---

## DEPENDENCIES

```
Phase 0 ✅ → Phase 1 ✅ → Phase 2 ✅ → Phase 2.5 (polish)
                                          ↓
                                     Phase 3 (intelligence)
                                          ↓
                                     Phase 4 (research)
                                          ↓
                                     Phase 5 (polish)
```

---

## IMMEDIATE NEXT STEPS

1. **Road Construction UI**
   - Add building mode to create roads between sects
   - Visual feedback during construction

2. **Transport Mode UI**
   - Click-to-select road
   - Mode toggle UI

3. **Integration Testing**
   - Test transport with multiple sects
   - Verify resource delivery

---

## TECHNICAL DEBT

| Priority | Item |
|----------|------|
| Low | Remove debug cout statements |
| Low | Add proper logging system |
| Medium | Save/load system |
| Medium | Unit testing framework |

---

**Next Review:** After Phase 2.5 transport polish
