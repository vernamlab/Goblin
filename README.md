# Goblin
With the advent of secure function evaluation (SFE), distrustful parties can jointly compute on their private inputs without disclosing anything besides the results. Yao's garbled circuit protocol has become an integral part of secure computation thanks to considerable efforts made to make it feasible, practical, and more efficient. For decades, the security of protocols offered in general-purpose compilers has been assured with regard to sound proofs and the promise that during the computation, no information on parties' input would be leaking. In a parallel effort, nevertheless, the vulnerability of garbled circuit frameworks to timing attacks has, surprisingly, never been discussed in the literature. This paper introduces Goblin, the first timing attack against commonly employed garbled circuit frameworks. Goblin is a machine learning-assisted, non-profiling, single-trace timing SCA, which successfully recovers the garbler's input during the computation under different scenarios, including various GC frameworks, benchmark functions, and the number of garbler's input bits. In doing so, Goblin hopefully paves the way for further research in this matter. 
For more information, please refer to [Garbled EDA: Privacy Preserving Electronic Design Automation](https://dl.acm.org/doi/abs/10.1145/3508352.3549455).
# Dependencies:
Install dependencies on Ubuntu:
g++: 
```
 $ sudo apt-get install g++
```
OpenSSL: 
```
  $ sudo apt-get install libssl-dev
```
boost:
```
  $ sudo apt-get install libboost-all-dev
```
cmake:
```
  $ sudo apt-get install software-properties-common
  $ sudo add-apt-repository ppa:george-edison55/cmake-3.x
  $ sudo apt-get update
  $ sudo apt-get upgrade
  $ sudo apt-get install cmake
```
TinyGarble:
```
  $ cd TintGarbe 
  $./configure
  $ cd bin
  $ make
```
