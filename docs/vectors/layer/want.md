# Layer `want` vectors

## The `want` op's clock and scheduler

A `want` vector runs on a clock and a scheduler the suite owns and hands the layer in place of the language's own. The
scheduler runs a task the layer gave it only when the suite moves time forward over the task's due time, at that
time, and never on its own. `fired` lists, in order, the clock's time each time the suite's scheduler actually ran a
callback the layer had scheduled and not cancelled: it witnesses what the timer did, where `timer` witnesses only what
the layer said it did. With `schedulerRunsBeforeRegistering` false, the suite moves its clock to `registerAtMillis`
without running any task its scheduler holds, so no timer of the layer's fires before the registration; absent, the
scheduler runs every task due on the way.

## `want/a-registration-at-the-deadline-loses`

A want whose deadline and a provider's registration fall at the same moment times out: the deadline is handed to
the core before the registration is made, so the registration settles nothing. A layer that runs the want on its
own clock must keep that order, because an event loop that delivers the registration first serves a want the
core has already timed out.

The suite builds the provider and the consumer through its layer, asks with the vector's timeout on a clock it
controls, registers the provider at `registerAtMillis`, and then witnesses:

- the want ends timed out at the deadline, exactly as `timedOut` and `atMillis` state, and is never served;
- the want's refusal is `timed_out`, its subject the service id it waited for (`accounts`), with a fix.

The suite's scheduler runs the task due on the way, so two things hand the core the deadline before the
registration: the timer firing at it, and the wrapper's check before the registration. Either alone keeps the order,
so the vector fails only a layer in which both are broken. The timer alone is witnessed by
`want/a-timed-want-arms-the-timer-at-its-deadline` and the check alone by
`want/a-registration-past-an-unfired-deadline-loses`; that each holds when the two fall due at the same moment is
held by review.

## `want/a-timed-want-arms-the-timer-at-its-deadline`

A layer arms its timer from the core's `WEIVEL_EVENT_DEADLINE` alone: the latest one drained is the deadline to arm,
and one that announces none disarms. It keeps no list of the calls that move a deadline, and it never advances the
core on a timer of its own reckoning.

The suite builds the consumer through its layer, asks with the vector's timeout on a clock it controls, lets nothing
register, runs the clock to `horizonMillis` without calling the core's advance itself, and records each move the
layer makes to its timer as `[at, deadline]`: the clock's time when the layer moved it, and the deadline it armed, or
null for a disarm. It then witnesses:

- `timer` exactly: armed at 1000 when the want was made at 0, and disarmed at 1000, once the want had timed out and
  the core announced that no deadline is left;
- the want ends timed out at 1000, its refusal `timed_out` with the subject `accounts` and a fix, because the timer
  firing at the announced deadline is what handed the core that time;
- `fired` exactly: the scheduler ran the layer's callback once, at 1000.

## `want/a-settled-want-disarms-the-timer`

A want that settles before its deadline takes the deadline with it, and the layer disarms on the event that says so.

The suite builds the provider and the consumer through its layer, asks with the vector's timeout on a clock it
controls, registers the provider at `registerAtMillis`, runs the clock to `horizonMillis`, records the timer's moves
as above, and witnesses:

- `timer` exactly: armed at 1000 when the want was made at 0, and disarmed at 500, when the registration settled the
  want, so the timer never fires;
- the want is served at 500 and never times out, with no refusal;
- `fired` exactly empty: the scheduler never ran a callback of the layer's, so a timer that reports the disarm but
  leaves its callback scheduled fails here.

## `want/a-registration-past-an-unfired-deadline-loses`

A layer hands the core a deadline that has fallen due before it makes any other call, whether or not its timer has
fired. The clock can pass the deadline before the scheduler takes its turn, and a registration made then must not
serve a want the core would have timed out: the wrapper reads its clock before the call and advances the core first.
The vector binds a layer only in a suite whose scheduler runs no task before the registration: in any other harness,
one that fires due tasks on the way among them, the timer hands the core the time and the wrapper's check is never
witnessed, so there the rule is held by review.

The suite builds the provider and the consumer through its layer, asks with the vector's timeout on a clock it
controls, moves the clock to `registerAtMillis` without running a task (`schedulerRunsBeforeRegistering` false),
registers the provider, runs the clock to `horizonMillis`, records the timer's moves as above, and witnesses:

- the want ends timed out when the registration is made, at 1500, never served, its refusal `timed_out` with the
  subject `accounts` and a fix;
- `timer` exactly: armed at 1000 when the want was made at 0, and disarmed at 1500 by the advance the layer made
  before the registration;
- `fired` exactly empty: the timer never fired, so only the layer's own check before the call handed the core the
  time.
