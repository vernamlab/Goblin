# Goblin
With the advent of secure function evaluation (SFE), distrustful parties can jointly compute on their private inputs without disclosing anything besides the results. Yao's garbled circuit protocol has become an integral part of secure computation thanks to considerable efforts made to make it feasible, practical, and more efficient. For decades, the security of protocols offered in general-purpose compilers has been assured with regard to sound proofs and the promise that during the computation, no information on parties' input would be leaking. In a parallel effort, nevertheless, the vulnerability of garbled circuit frameworks to timing attacks has, surprisingly, never been discussed in the literature. This paper introduces Goblin, the first timing attack against commonly employed garbled circuit frameworks. Goblin is a machine learning-assisted, non-profiling, single-trace timing SCA, which successfully recovers the garbler's input during the computation under different scenarios, including various GC frameworks, benchmark functions, and the number of garbler's input bits. In doing so, Goblin hopefully paves the way for further research in this matter. 
For more information, please refer to [Time is money, friend! Timing Side-channel Attack against Garbled Circuit Constructions](https://eprint.iacr.org/2023/001.pdf).
```
@article{hashemi2023time,
  title={Time is money, friend! Timing Side-channel Attack against Garbled Circuit Constructions},
  author={Hashemi, Mohammad and Forte, Domenic and Ganji, Fatemeh},
  journal={Cryptology ePrint Archive},
  year={2023}
}
```
## Available codes:
To use Goblin, please refer to our GitHub repository: [Goblin package](https://github.com/vernamlab/Goblin)
## What are the incredible features of Goblin?
Goblin is the first non-profiling, single-trace timing SCA that successfully extracts the user’s input, which, by definition, should have been kept secret.
## How easy is it to use Goblin, and how scalable is Goblin?
Goblin is machine-learning assisted in disclosing the garbler’s input, regardless of size. For this purpose, k-means clustering is applied, where no manual tuning or heuristic leakage models are needed. It is, of course, advantageous to the attacker and allows for scalable and efficient attacks.
## Which garbled circuit optimizations are vulnerable to Goblin?
The free-XOR- and half-gates-optimized constructions are vulnerable to Goblin attack.
## Why free-XOR- and half-gates-optimized constructions are vaulnarable to Goblin?
The existence of these unbalanced IFs demonstrates the likelihood of timing attacks to be successfully mounted against them.
## How did we examine the possibility of mounting timing SCA against GC frameworks?
To examine this, SC-Eliminator [1] is applied against TinyGarble [2], JustGarble [3], EMP-toolkit [4], Obliv-C [5], and ABY [6], and here is the leaky IF reports:
Table
## 
