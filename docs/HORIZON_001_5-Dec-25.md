# HORIZON 001 | 5-Dec-25
## Planned Features & Progression

---

## PHASE 0: Foundation (75% → 100%)
**Status:** Wrapping up

### Remaining
- [ ] **Typed Resources** - MACHINERY, ELECTRONICS with subtypes/quality
- [ ] **Population System** - Sect generates MANPOWER from population
- [ ] **Ambient Energy** - Solar intensity based on time of day

---

## PHASE 1: Core Resource Flow
**Status:** Next up

### 1.1 Storage & Overflow
- Enforce capacity limits at all levels
- Overflow buffers with backpressure
- Storage upgrade mechanics

### 1.2 Flow Mechanisms
- Unit → Sect: Direct deposit (done)
- Sect → Colony: Surplus push at 80% (done)
- Colony → Sect: Deficit pull (planned)
- Transport timing calculations

### 1.3 Graceful Degradation
- Partial operation when resources scarce
- Module efficiency reduction
- Emergency shutdown protocols

### 1.4 Resource Allocator
- Priority-based distribution
- Scarcity handling
- Request queue system

---

## PHASE 2: Transport Network
**Status:** Planned

### 2.1 Network Core
- TransportNetwork class
- TransportJob struct (source, dest, resource, amount)
- A* pathfinding on road network
- Distance/speed calculations

### 2.2 Dynamic Parameters
- Technology-based speed multipliers
- Transport module effects
- Capacity limits per route

### 2.3 Auto-Scheduling
- Smart routing (closest surplus → deficit)
- Player-adjustable priorities
- Job visualization

### 2.4 Visuals
- Animated resource packets on roads
- Color-coded by resource type
- Progress indicators
- Traffic congestion display

---

## PHASE 3: Production Intelligence
**Status:** Planned

### 3.1 Strategy Pattern
- ProductionStrategy base class
- Unit-specific strategies (Extraction, Farming, Energy...)
- Clean separation of production logic

### 3.2 Smart Distribution
- ResourceDistributionPolicy class
- Player action tracking
- Priority learning from behavior
- Adaptive Energy/Manpower allocation

### 3.3 Policy Framework
- Policies: SURVIVAL, GROWTH, RESEARCH, EXPANSION
- Auto-balancing system
- Production suggestions

### 3.4 Module Chaining
- Internal buffers between modules
- Dependency chains (output A → input B)
- Vertical integration within units

---

## PHASE 4: Module Enhancement
**Status:** Planned

### 4.1 Expanded Modules
- 3-5 modules per unit type
- Specialized variants (H2/O2/Fe/Si extractors)
- Module tech tree

### 4.2 Module Dependencies
- Prerequisites to unlock
- Level-based unlocks
- Quality requirements

### 4.3 Module UI
- Selection interface
- Status indicators
- Efficiency visualization
- Tooltip system

---

## PHASE 5: Research & Technology
**Status:** Planned

### 5.1 Research System
- ResearchManager class
- SCIENCE accumulation at colony level
- Research queue

### 5.2 Tech Tree
- Prerequisites between techs
- Unlock effects (modules, units, bonuses)
- Colony-wide bonuses (transport speed, efficiency)

### 5.3 Research UI
- Tree visualization
- Progress display
- Unlock indicators

---

## PHASE 6: Polish & Optimization
**Status:** Future

### 6.1 Performance
- Profile resource system
- Optimize tick processing
- Spatial partitioning for rendering
- LOD system

### 6.2 UI/UX
- Resource dashboard
- Contextual tooltips
- Keyboard shortcuts
- Notification system
- Settings menu

### 6.3 Visual/Audio
- Particle effects for production
- Smooth camera transitions
- Sound effects
- Background music

### 6.4 Onboarding
- Tutorial sequence
- Contextual hints
- Achievement system

---

## PHASE 7: Advanced Features (Post-Launch)
**Status:** Ideas

- Multi-colony trade
- Planetary events/disasters
- Advanced automation
- Multiplayer/co-op
- Procedural scenarios
- Combat/defense
- Orbital structures

---

## IMMEDIATE NEXT STEPS

1. **TypedResource struct**
   - baseType, subType, quality, efficiency
   - MACHINERY: HeavyDrill, Conveyor, Assembler
   - ELECTRONICS: Sensor, Controller, Computer

2. **Population in Sect**
   - `float population` member
   - Generates MANPOWER (1% per tick)
   - Consumes FOOD + WATER
   - Growth/decline based on supply

3. **Ambient Energy**
   - Solar intensity by time of day
   - Base energy generation
   - Weather effects (future)

---

## DEPENDENCIES

```
Phase 0 ──► Phase 1 (resource flow needs capacity system)
            │
            ├──► Phase 2 (transport needs flow working)
            │
            └──► Phase 3 (strategies need flow working)
                    │
                    └──► Phase 4 (modules need strategies)
                            │
                            └──► Phase 5 (research unlocks modules)
                                    │
                                    └──► Phase 6 (polish after features)
```

---

## RISKS

| Risk | Impact | Mitigation |
|------|--------|------------|
| Resource contention | Unpredictable module conflicts | Priority system, pause if starved |
| Overflow cascades | Chain reaction up hierarchy | Backpressure, pause production |
| Performance at scale | Lag with many units | Profiling, LOD, spatial partitioning |
| Complexity creep | Hard to balance | Incremental features, playtesting |

---

**Next Review:** After Checkpoint 4 & 5 complete
