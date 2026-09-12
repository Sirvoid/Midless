/**
 * Copyright (c) 2026 Sirvoid
 * Released under the MIT License. https://opensource.org/licenses/MIT
 */
#ifndef MIDLESS_SERVER_TIMING_H
#define MIDLESS_SERVER_TIMING_H

// Both hosts service the bounded streaming queues between simulation ticks.
#define SERVER_SERVICE_WAIT_SECONDS 0.001
#define SERVER_SIMULATION_INTERVAL_SECONDS (1.0 / 60.0)

static inline double ServerTiming_SimulationStep(double now, double *previous) {
    double elapsed = now - *previous;
    if (elapsed < SERVER_SIMULATION_INTERVAL_SECONDS) return 0;
    *previous = now;
    // Avoid a burst of catch-up work after a stall.
    return elapsed > 0.25 ? 0.25 : elapsed;
}

#endif
