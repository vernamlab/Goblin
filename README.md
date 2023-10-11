# Goblin
<p align="center">
<img src="https://github.com/vernamlab/Goblin/blob/Website/noun-goblin-5218422-007435.png" alt="drawing" width="200" align="center"/>
</p>
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
<div align="center">
|          Framework         | IF |
|:--------------------------:|:--:|
| TinyGarble [2] (half-gate) |  4 |
|  TinyGarble [2] (free-XOR) |  7 |
|       JustGarble [3]       | 11 |
|       EMP-toolkit [4]      |  0 |
|         Obliv-c [5]        |  4 |
|           ABY [6]          |  0 |
</div>
# References:
1. Wu, M., Guo, S., Schaumont, P., Wang, C.: Eliminating timing side-channel leaks using program repair. In: Proceedings of the 27th ACM SIGSOFT International
Symposium on Software Testing and Analysis. pp. 15–26 (2018).
1. Ebrahim M. Songhori, Siam U. Hussain, Ahmad-Reza Sadeghi, Thomas Schneider and Farinaz Koushanfar, "TinyGarble: Highly Compressed and Scalable Sequential Garbled Circuits." Security and Privacy, 2015 IEEE Symposium on May, 2015.
1. Bellare, Mihir, et al. "Efficient garbling from a fixed-key blockcipher." 2013 IEEE Symposium on Security and Privacy. IEEE, 2013.
1. Malozemoff, A., Wang, X., Katz, J.: Emp-toolkit framework. [Online]https://
github.com/emp-toolkit [Accessed Jan.30, 2023] (2022).
1. Zahur, Samee, and David Evans. "Obliv-C: A language for extensible data-oblivious computation." Cryptology ePrint Archive (2015).
1. Demmler, Daniel, Thomas Schneider, and Michael Zohner. "ABY-A framework for efficient mixed-protocol secure two-party computation." NDSS. 2015.
