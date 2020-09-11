# The Paxos Algorithm Demo

This example was provided for study purpose only!
Sunding Wei, weisunding@gmail.com

## How to use
- You should change the NODE_COUNT to your desired process count
- make
- Start the process with command: ./node N

## Default example output with 3 processes

```
❯ ./node 1
...

❯ ./node 2
...

❯ ./node 3
Paxos Demo by Sunding Wei (PN = Proposal Number)
Node 3 is running at port: 5003
   send prepare to node 1, [PN: 9]
   send prepare to node 2, [PN: 9]
   send prepare to node 3, [PN: 9]
   ...
Restart since no consensus
   send prepare to node 1, [PN: 13]
   send prepare to node 2, [PN: 13]
   send prepare to node 3, [PN: 13]
   send promise to node 3, [PN: 13]
   recv promise from node: 3, accepted [PN: 12]
   recv promise from node: 1, accepted [PN: 12]
   I propose [PN: 13,  value: 3080]
   send accept to node 1, [PN: 13, value: 3080]
   send accept to node 3, [PN: 13, value: 3080]
   recv accept from node 3, [PN: 13, value: 3080]
   recv OK from node: 3, [PN: 13, value: 3080]
   recv promise from node: 2, accepted [PN: 11]
   recv OK from node: 1, [PN: 13, value: 3080]
   As proposer, chosen (node: 3, [PN: 13, value: 3080])
   notify node 1, proposal (node: 3, [PN: 13, value: 3080])
   notify node 2, proposal (node: 3, [PN: 13, value: 3080])
   notify node 3, proposal (node: 3, [PN: 13, value: 3080])
   recv notify from node: 3, [PN: 13, value: 3080]
Chosen (node: 3, [PN: 13, value: 3080])
```
