
This document outlines all guidelines, rules, and procedures that participants must follow throughout the competition. Please read this document carefully to ensure compliance with competition standards.

# Arena:

### Auto

The maze are divided into 2 separate part: A(Red alliance), B(Blue alliance) along the left and the right side of the arena.

!Screenshot 2026-09-22 172302.png

`Note`: Each block of the maze are `20 x 20 cm`  

`Note`: You’ll need to handle 2 sides of the arena

### Manual

The competition arena of manual robot will be based of MakeX Explorer competition 2023 consisted of a map and frame. It is a rectangular area with a size of `2443 x 2215 mm` with frame's height of `15 cm` divided into two zones for each alliance: zone A (Red) and zone B (Blue).

!Screenshot 2568-10-07 at 10.54.16.png

`Full map Reference`:  2022-2023-Season-MakeX-Explorer-Eco-Pioneer-Rules-Guide-V2.1.pdf

### Full Map

!ดีไซน์ที่ยังไม่ได้ตั้งชื่อ.png

## Component and Maps Description

**Objective Block(Auto):** These are the objective object that you’ll deliver it to the parking zone for each alliance to scored a point for auto phase.

!Screenshot 2568-10-07 at 10.51.19.png

`Note`: The cube dimensions are: `7 x 7 x 7 cm`

**Cube Base Platform(Manual):** the platform is where the manual robot places the yellow cube to score. It is mounted beside the goalpost. 

!image.png

!image.png

`Note`: The platform dimensions are: `10 x 10 x 30 cm`  with edge thickness 0.3 cm and edge height 1 cm.

**Initial position marker(Manual):** These positions are the positions where the manual robots will be placed and able  to retract in at the start of the competition.

!Screenshot 2568-10-07 at 10.22.36.png

**Goalpost(Manual)**: to score with a ball, you must shoot it in here to be counted

!Screenshot 2568-10-07 at 10.52.27.png

!image.png

**Ball(Manual)**: The diameter of small yellow ball and large green ball is 70mm and 100mm respectively.

!Screenshot 2568-10-07 at 11.02.22.png

**Target pins(Manual):** The red/blue pins are placed on top of the goal to be shoot down and earn a point. 

The height of each pin are `250 mm`, the diameter of the bottom of each
pin is `70 mm`, and the maximum diameter is `100 mm`.

!image.png

**Reversed Flag(Manual): t**he reversed flag area is located in the upper layer of the left and right part of the central resource area. 

!IMG_5075.JPG

!Asm Drawing v1.jpg

**Central area(Manual)**:

!Screenshot 2568-10-07 at 22.06.20.png

# Programming specification

Your source code for both of your robot(auto & manual) must be stored on a **`team leader's GitHub repository`** in the following structure:

```css
$> tree
.
├── README.md
├── ...
└── competition
		├── auto
		│		├── auto.ino
		│   └── ...
    └── manual
		    └── your_manual_code.mblock
```

### Autonomous robot code specification

- Your code must contains a use of function, structure, loop, array and pointer
- You’re allowed to hardcode your solution (Good luck doing that)

### Manual robot code specification

- You must write all of your logic in MBlock’s python only (A use of block code is **FORBIDDEN**)

You must also provide a flowchart and a detailed explanation of your code in the report. 

## Code submission

Every team must submit their code before `11 November 23:59` via a team leader’s GitHub repository.

This must be the same repository described above, and it must be `public`, so the TA can access and review it at any time. 

# Robot specification

Every group must have their own autonomous robot that must be able to navigate through the maze and reach the goal. The robot must be designed to be able to grab an object and deliver to the target zone. 

The groups must also prepare a manual robot which is operated by the remote control.

**You can view the Component Lists via this link:**

X

### Autonomous robot

- Your autonomous robot must have a maximum dimensions of `16 cm x 16 cm x inf` (L * W * H)
- You’re allowed to use all of your components from the given Autonomous Robot Set (you can check from the component lists)
- You’re allowed to use only **5 sensors maximum** (Any kind of sensor is acceptable)
- You’re allowed to use only **2 Yellow DC Motor maximum(Can use only motor that TA provide)**
- You’re allowed to `build`your mechanical components part of the robot
- You’re `not allowed to buy` complete set of Autonomous Robot kit
- Any harm to the given components are **FORBIDDEN** (-5 pts. per any damaged part)
    - Ex. Use of any kind of glue, sticky tape, Cutting or drilling that damages the given parts
- All mechanical part and anything in Makebox kit must be returned back in perfect condition after competition (-5 pts. per any damaged/lost part + you’ll need to buy back what you damaged/lost)

### Manual robot

- Your manual robot must have a maximum dimensions of `32 cm x 32 cm x 45 cm` (L * W * H) when fully extend. However, the robot could extends itself beyond this boundary during the manual phase.
- You’re only allowed to use all of your components from the given Manual Robot set(Any external electronics is **FORBIDDEN** on manual robot, you can check from the component lists)
- You’re allowed to ONLY 1 **additional DC Motor maximum**
- You’re allowed to `build`your mechanical components part of the robot
- You’re `not allowed to buy` complete set of Manual Robot kit
- Any harm to the given components are **FORBIDDEN** (-5 pts. per any damaged part)
    - Ex. Use of any kind of glue, sticky tape, Cutting or drilling that damages the given parts
- All mechanical part and anything in Makebox kit must be returned back in perfect condition after competition (-5 pts. per any damaged/lost part + you’ll need to buy back what you damaged/lost)

# GAMEPLAY

### Robot inspection

Before`every match,` each robot must fit inside its acrylic sizing box. One for autonomous (`16 x 16 x inf`), one for manual (`32 x 32 x 45 cm`, fully retracted). If it doesn't fit, the team must fix it and get re-checked before the match starts.

## Ranking match

1. Each team plays its ranking match alone, with no opponent. The team collects points from every completed task until the time runs out; the ranking score is the total of these points.
2. The autonomous robot starts at the maze `entrance 1 (A)`, already holding the yellow cube.
3. With the signal from the referee, the game will begin, and the first `90 seconds` (Auto phase) are given to the autonomous robot to carry the cube through the maze and park in the Parking Zone.
    1. The autonomous robot must hold the cube and must not place it on the floor.
    2. The referee announces when the autonomous robot is parked.
4. If the cube drops before the autonomous robot reaches the Parking Zone, the team may choose to:
    1. Relaunch at the drawn maze entrance 1 (A) with a new cube. The referee removes the dropped cube.
    2. Continue and park in the Parking Zone without the cube.
5. Relaunch during the Auto phase
    1. A relaunch can only be conducted within `90 seconds` of the Auto phase and only at the drawn maze entrance 1 (A).
    2. The team is allowed to relaunch as many times as they want before the Auto phase ends. The timer does not stop.
    3. The team must notify the referee before relaunching.
    4. One team member may carry the autonomous robot back to the maze entrance 1 (A). This is not counted as touching the robot.
    5. If the robot malfunctions, the teammates are allowed to fix the robot before relaunching.
    6. During a relaunch, the `manual`robot must stay still at its current position.
6. Manual robot after Autonomous Robot parked in the Parking Zone 
    1. The manual robot is allowed to move once the referee announces that the autonomous robot has parked while holding the cube.
    2. If the autonomous robot parks without the cube, the manual robot `must wait` until the Auto phase ends(90 secs).
    3. The manual robot must take the cube directly from the autonomous robot's mechanism. Picking up the cube from the floor of the Auto field, including the Parking Zone Floor, does not score.
    4. After taking the cube, the manual robot may place it on the Cube Base platform(Manual) and continue scoring with the balls or the reverse flag. 
7. Cube drops during the handoff
    1. If the cube drops inside the Auto field, including the Parking Zone, the referee will `PAUSE`the game and allowed 1 member from that team to reload the cube on the parked autonomous robot in the Parking Zone. (While the referee returns the cube, the manual robot must stay still.)
    2. If the cube drops in the Manual field, the team can pick up the cube from the Manual field floor with the manual robot and place it on the Cube Base platform(Manual), Alliance’s team can help pick up.
8. End of the Auto phase (`90 seconds`)
    1. After `90 seconds,`The Auto phase ends immediately. Parking points can no longer be scored. any action of the autonomous robot does not score.
    2. A cube still held by the autonomous robot is out of play and will be removed by the referee. **`A cube in the Manual field stays in play.`**
    3. The team must stop the autonomous robot by either letting it stop by itself through its program, or having one team member stop or remove it right after the time-up signal. Team members may only enter the Auto field.
    4. If the autonomous robot is still moving, the referee will order the team to stop it. If it is not stopped within `10 seconds` of the order, the team loses 5 points.
    5. No one is allowed to enter or reach into the Manual field at any time.
    6. When the time runs out, the Auto field will be reset.
9. The Manual phase always starts at the end of the Auto phase and lasts `120 seconds`. Within this time, the manual robot can perform these tasks to score:
    1. Grab the ball from the center area and launch it to the opposing goal and bowling pins.
    2. Grab the ball from the center area and delivery robot and launch it to the reverse flag.
    3. Place the cube on the  Cube base Platform(Manual). The bottom of the cube must `lie flat inside the platform edge`. A cube resting on or `tilted over the edge scores 0` points.
    4. If the cube falls off the Cube Base platform(Manual), the manual robot may pick it up from the Manual field floor and place it again. If the cube leaves the arena, the referee places it back into the autonomous robot parked in the Parking Zone.
    5. `Cube Base Platform(Manual) points are counted at the end of the game.`At that moment, no robot or person may be touching the cube.
    6. After the game is finished, all balls must return to their original position. If not returned (-5 points).
10. After all ranking matches, the score will be calculated, and teams with a higher score can choose a lower-score team to be their alliance for the final match.
11. If teams have the same ranking score, the team with more Auto–Manual sync points ranks higher. If still tied, the referee decides.

## Final match

1. The final match is a tournament match between two alliances, one on each side of the arena: A (Red alliance) and B (Blue alliance). Both alliances play at the same time, and the alliance with more points at the end of the game wins.
2. Before the match, each alliance draws its maze entrance at `random`. The autonomous robot starts at the drawn maze entrance, already holding the yellow cube.
3. With the signal from the referee, the game will begin, and the first `90 seconds` (Auto phase) are given to each alliance's autonomous robot to carry the cube through the maze and park in the Parking Zone.
    1. The autonomous robot must hold the cube and must not place it on the floor.
    2. The referee announces when the autonomous robot is parked.
4. If the cube drops before the autonomous robot reaches the Parking Zone, the alliance may choose to:
    1. Relaunch at the drawn maze entrance with a new cube. The referee removes the dropped cube.
    2. Continue and park in the Parking Zone without the cube.
5. Relaunch during the Auto phase
    1. A relaunch can only be conducted within `90 seconds` of the Auto phase and only at the drawn maze entrance(Same as what was randomly drawn the first time of the final match).
    2. The alliance is allowed to relaunch as many times as they want before the Auto phase ends. The timer does not stop.
    3. The alliance must notify the referee before relaunching.
    4. One team member may carry the autonomous robot back to the maze entrance(Same as what was randomly drawn the first time of the final match). This is not counted as touching the robot.
    5. If the robot malfunctions, the teammates are allowed to fix the robot before relaunching.
    6. During a relaunch, the manual robot must stay still at its current position.
6. Manual robot after Autonomous Robot parked in the Parking Zone 
    1. The manual robot is allowed to move once the referee announces that its alliance's autonomous robot has parked while holding the cube.
    2. If the autonomous robot parks without the cube, the manual robot `must wait` until the Auto phase ends(90 secs).
    3. The manual robot must take the cube directly from the autonomous robot's mechanism. Picking up the cube from the floor of the Auto field, including the Parking Zone, does not score.
    4. After taking the cube, the manual robot may place it on the platform and continue scoring with the balls and the reverse flag.
7. Cube drops during the handoff
    1. If the cube drops in the Auto field, including the Parking Zone, the referee will `PAUSE`the game and allowed 1 member from the Alliance’s team to reload the cube on the parked autonomous robot in the Parking Zone.
    2. If the cube drops in the Manual field, the alliance may pick up the cube from the Manual field floor with the manual robot and place it on the Cube Base platform(Manual). 
8. End of the Auto phase (`90 seconds`)
    1. After `90 seconds,`The Auto phase ends immediately. Parking points can no longer be scored. any action of the autonomous robot does not score.
    2. A cube still held by the autonomous robot is out of play and will be removed by the referee. **`A cube in the Manual field stays in play.`**
    3. The alliance must stop the autonomous robot by either letting it stop by itself through its program, or having one team member stop or remove it right after the time-up signal. Team members may only enter the Auto field.
    4. If the autonomous robot is still moving, the referee will order the alliance to stop it. If it is not stopped within `10 seconds` of the order, the alliance loses 5 points.
    5. No one is allowed to enter or reach into the Manual field at any time.
    6. When the time runs out, the Auto field will be reset.
9. The Manual phase always starts at the end of the Auto phase and lasts `120 seconds` for both alliances. Within this time, the manual robot can perform these tasks to score:
    1. Grab the ball from the center area and launch it to the opposing goal and bowling pins.
    2. Grab the ball from the center area and delivery robot and launch it to the reverse flag.
    3. Place the cube on the Cube Base Platform(Manual). The bottom of the cube must `lie flat inside the platform edge.` A cube resting on or `tilted over the edge scores 0`points.
    4. The `opposing`alliance `may shoot` the cube off the Cube Base Platform(Manual). The alliance must keep the cube on the platform until the end of the game.
    5. If the cube falls off the platform, the manual robot may pick it up from the Manual field floor and place it again. If the cube leaves the arena, the referee places it back into the autonomous robot parked in the Parking Zone.
    6. `Cube Base Platform(Manual) points are counted at the end of the game.`At that moment, no robot or person may be touching the cube.
    7. After the game is finished, all balls must return to their original position. If not returned (-5 points).
10. At the end of the game, the alliance with more points wins and goes to the next round.
11. If the match is tied, the winner will be determined by whichever alliance used less time in the autonomous period.

# Scoring

## Auto phase

| Action | Rewarded point | Remarks |
| --- | --- | --- |
| Park in the Parking Zone without the cube | +5 | Only the higher of the two parking actions is counted |
| Park in the Parking Zone while holding the cube | +10 | The cube must be lifted off the floor. Replaces the +5 above, not added to it (The maximum is +10 ) |
| Hand off and place the cube on the Cube Base Platform(Manual) | +15 | The manual robot must take the cube directly from the autonomous robot, or pick it up from the Manual field floor after it drops there. Picking it up from the Auto field floor or the Parking Zone does not score. Counted at the end of the game |

## Manual phase

| Action | Rewarded point | Remarks |
| --- | --- | --- |
| Pin knockdown | +5 per pin | - |
| Reverse flag flipped | +5 per flag | The reverse flag must be flipped via ball throwing |
| Scores with the green ball | +5 per ball | The ball must be in the goalpost to be counted |
| Scores with the yellow ball | +2.5 per ball | The ball must be in the goalpost to be counted |
| Pick up ball from goal | -5.0 per ball | - |
| Balls not returned to original position after the game | -5 | - |

## Penalties

| Violation | Penalty | Remarks |
| --- | --- | --- |
| Manual robot moves before the referee's announce to start Manual Phase | -5 | The manual robot must return to its starting point |
| A person touches the cube or a robot during the match | Platform points void | Does not apply to carrying the robot during a relaunch or stopping the autonomous robot after the time-up signal |
| Autonomous robot not stopped within 10 seconds of the referee's order | -5 | - |