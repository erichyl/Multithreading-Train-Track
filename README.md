## 1 Goals of this Assignment

Your goal is to construct a simulator of an automated control system for the
railway track shown in Figure 1 (i.e., to emulate the scheduling of multiple
threads sharing a common resource in a real OS).

![](./figures/figure1.png)

As shown in Figure 1, there are two stations on each side of the main track.
The trains on the main track have two priorities: high and low.

At each station, one or more trains are loaded with commodities. Each train
in the simulation commences its loading process at a common start time 0 of
the simulation program. Some trains take more time to load, some less. After
a train is loaded, it patiently awaits permission to cross the main track,
subject to the requirements specified in Section 3. Most importantly, only
one train can be on the main track at any given time. After a train finishes
crossing, it magically disappears (i.e., the train thread finishes). 

You need to use `threads` to simulate the trains approaching the main track
from two different directions, and your program will schedule between them
to meet the requirements in Section 3.

## 2. Trains.

Each train, which will be simulated by a thread, has the following attributes:

1. `Number`: An integer uniquely identifying each train, starting from `0`.
2. `Direction`:
    + If the direction of a train is `Westbound`, it starts from the East
    station and travels to the West station.
    + If the direction of a train is `Eastbound`, it starts from the West
    station and travels to the East station.
3. `Priority`: The priority of the station from which it departs.
4. `Loading Time`: The amount of time that it takes to load it (with goods)
before it is ready to depart.
5. `Crossing Time`: The amount of time that the train takes to cross the main 
track.

Loading time and crossing time are measured in 10ths of a second. These durations
will be simulated by having your threads, which represent trains, `usleep()` for
the required amount of time.

### 2.1. Reading the input file

Your program (`mts`) will accept only one command line parameter:

+ The parameter is the name of the input file containing the definitions of the 
trains.

#### 2.1.1. Input file format

The input files have a simple format. Each line contains the information about a
single train, such that:

+ The first field specifies the direction of the train. It is one of the following
four characters:
`e`, `E`, `w`, or `W`.
    + `e` or `E` specifies a train headed East (East-Bound): `e` represents an
    east-bound low-priority train, and `E` represents an east-bound high-priority
    train;
    + `w` or `W` specifies a train headed West (West-Bound): `w` represents a
    west-bound low-priority train, and `W` represents a west-bound high-priority
    train.
+ Immediately following (separated by a space) is an integer that indicates the
loading time of the train.
+ Immediately following (separated by a space) is an integer that indicates the
crossing time of the train.
+ A newline (\n) ends the line.

Trains are numbered sequentially from 0 according to their order in the input file.

Note: You may assume that no more than `75` trains will be provided.

## 3 Simulation Rules

The rules enforced by the automated control system are:
+ Only one train is on the main track at any given time.
+ Only loaded trains can cross the main track.
+ If there are multiple loaded trains, the one with the high priority crosses.
+ If two loaded trains have the same priority, then:
    + If they are both traveling in the same direction, the train which finished 
    loading first gets the clearance to cross first. If they finished loading at 
    the same time, the one that appeared first in the input file gets the clearance 
    to cross first.
    + If they are traveling in opposite directions, pick the train which will travel 
    in the direction opposite of which the last train to cross the main track traveled. 
    If no trains have crossed the main track yet, the Westbound train has the priority.
+ To avoid starvation, if there are two trains in the same direction traveling through
the main track **back to back**, the trains waiting in the opposite direction get a
chance to dispatch one train if any.

### 3.1 Output

For the example, shown in Section 2.1.2, the correct output is:

```
00:00:00.3 Train  2 is ready to go East
00:00:00.3 Train  2 is ON the main track going East
00:00:00.6 Train  1 is ready to go West
00:00:01.0 Train  0 is ready to go East
00:00:01.3 Train  2 is OFF the main track after going East
00:00:01.3 Train  1 is ON the main track going West
00:00:02.0 Train  1 is OFF the main track after going West
00:00:02.0 Train  0 is ON the main track going East
00:00:02.6 Train  0 is OFF the main track after going East
```

You must:

+ print the arrival of each train at its departure point (after loading)
using the format string, prefixed by the simulation time:
    ```
    "Train %2d is ready to go %4s"
    ```
+ print the crossing of each train using the format string, prefixed by the
simulation time:
    ```
    "Train %2d is ON the main track going %4s"
    ```
+ print the arrival of each train (at its destination) using the format string,
prefixed by the simulation time:
    ```
    "Train %2d is OFF the main track after going %4s"
    ```

where

+ There are only two possible values for direction: "East" and "West"
+ Trains have integer identifying numbers. The ID number of a train is specified
implicitly in the input file. The train specified in the first line of the input
file has ID number 0.
+ Trains have loading and crossing times in the range of `[1, 99]`.
+ Train crossing times are accurate within 00:00:00.1. That is to say:
00:00:00.1 variation is permitted, to accomodation rounding errors
and system clock inconsistencies. 