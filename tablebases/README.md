# Ultimate Fish tablebases

These bundled files are exact WDL/DTW retrograde solutions for the native
8x10 board. Position-only single-character classes use a dense 985,920-state
codec with both Kings and the named Ivory character on distinct anchor squares,
either side to move, and every model `moved=true`. The moved-state restriction
makes the class closed by excluding castling; the probe rejects state not
represented by that class.

<!-- GENERATED_TABLE_START -->
| File | Class | In-class edges | First material owner starts W / L / D | Second material owner / bare King starts W / L / D | SHA-256 |
| --- | --- | ---: | ---: | ---: | --- |
| `kjesterk.uftb` | King+Jester vs King | 9,169,752 | 492,960 / 0 / 0 | 41,808 / 414,344 / 36,808 | `3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa` |
| `kpawnk.uftb` | King+Pawn vs King | 12,759,470 | 678,502 / 0 / 307,418 | 0 (83,616) / 459,272 / 443,032 | `42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844` |
| `kqk.uftb` | King+Queen vs King | 15,744,492 | 492,960 / 0 / 0 | 0 (41,808) / 413,304 / 37,848 | `1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3` |
| `krk.uftb` | King+Rook vs King | 11,970,912 | 492,960 / 0 / 0 | 0 (41,808) / 414,300 / 36,852 | `abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44` |
| `kberserkerk.uftb` | King+Berserker vs King | 92,321,028 | 4,929,600 / 0 / 0 | 0 (418,080) / 4,143,200 / 368,320 | `f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1` |
| `kbombk.uftb` | King+Bomb vs King | 9,761,788 | 492,960 / 0 / 0 | 0 (41,808) / 450,360 / 792 | `3d4f44035652e486cbd72c59e8247cfbba355b748107ee2b9d06abfe4674b864` |
| `kninjak.uftb` | King+Ninja vs King | 12,610,592 | 492,960 / 0 / 0 | 0 (41,808) / 413,376 / 37,776 | `4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776` |
| `kghostk.uftb` | King+Ghost vs King | 17,761,144 | 985,920 / 0 / 0 | 0 (83,616) / 865,512 / 36,792 | `3be39c5ab2bfec00cb9dd500e26911bd145bcb1f4dde77fd2c84ef33d111fc31` |
| `kpenguink.uftb` | King+Penguin vs King | 39,165,400 | 289,648 / 8,992 / 5,616,880 | 19,576 (261,408) / 10,560 / 5,623,976 | `bec15406a45c4a955e5ca35a0190e3447abd7c68f621de254b9fc6afe242f8f3` |
| `kparasitek.uftb` | King+Parasite vs King | 8,602,716 | 492,960 / 0 / 0 | 0 (41,808) / 451,120 / 32 | `f08b2676a703259ef638bc4ab72b9cd7e6b2000afc72dd70b0ac17e03ea858ab` |
| `ksniperk.uftb` | King+Sniper vs King | 24,180,908 | 199,566 / 0 / 1,772,274 | 0 (167,232) / 2,276 / 1,802,332 | `473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5` |
| `kprincek.uftb` | King+Prince vs King | 14,322,440 | 122,400 / 38,536 / 824,984 | 0 (41,808) / 44,792 / 899,320 | `3b1b965e31878b19e9c930ee8c8bdf256c1445c1010ab4e6b53b991120b28b0f` |
| `kgiantk.uftb` | King+Giant vs King | 4,747,504 | 85,776 / 0 / 407,184 | 0 (30,868) / 1,460 / 460,632 | `eb52f2c08cf88e1e3682d0c72dfde191d9009e79779ad7eee23ca82fdcade591` |
| `kcopycatk.uftb` | King+Copycat vs King | 10,685,864 | 480,408 / 0 / 12,552 | 0 (40,776) / 374,136 / 78,048 | `98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb` |
| `kdragonk.uftb` | King+Dragon vs King | 11,904,156 | 492,960 / 0 / 0 | 0 (41,808) / 414,164 / 36,988 | `28d3cbeba82d02611a48bf2d0a6a527d11ff4cd3049f04bf2b4b929a05ed86c6` |
| `kjesterjesterk.uftb` | King+2 Jesters vs King | 231,295,032 | 9,489,480 / 0 / 0 | 804,804 / 8,682,564 / 2,112 | `791453682cab1999efeeea44cc79b7d84be3b2658975a0e004cf589ca891e150` |
| `kjesterkjester.uftb` | King+Jester vs King+Jester | 489,320,832 | 6,639,760 / 493,966 / 11,845,234 | 6,639,760 / 493,966 / 11,845,234 | `243f0eaaf838774684082dadfeb69496b200e0061514239a83ccb771b879deee` |
| `kjesterknightk.uftb` | King+Jester+Knight vs King | 441,170,696 | 18,978,960 / 0 / 0 | 1,609,608 / 16,055,072 / 1,314,280 | `4d9e7cd0ad349ef4ddc10f214ba3d5e63000d551b307a4e0e5266ee3950de9e6` |
| `kjesterkknight.uftb` | King+Jester vs King+Knight | 432,640,208 | 5,991,648 / 4 / 12,987,308 | 2,808,980 / 121,472 / 16,048,508 | `cf6d15681c176ff90cd7c862187b5d1ab4db217c6b00468f032f57c817155d69` |
| `kjesterqueenk.uftb` | King+Jester+Queen vs King | 751,025,764 | 18,978,960 / 0 / 0 | 1,609,608 / 17,307,456 / 61,896 | `b1e938905b645a70b2a2de433eb0ea12dc1374ad0bdef79738f115b65adfcb74` |
| `kjesterkqueen.uftb` | King+Jester vs King+Queen | 712,427,800 | 5,582,532 / 7,673,186 / 5,723,242 | 16,716,862 / 2,682 / 2,259,416 | `1345e2b73a1978a6569821a379c236415997ff3b9cdb108563c982f458d88600` |
| `kjesterrookk.uftb` | King+Jester+Rook vs King | 595,252,940 | 18,978,960 / 0 / 0 | 1,609,608 / 17,360,968 / 8,384 | `9e485cf5516add2e833527fbce24374380f444322b0db1e5a41e687bda500c82` |
| `kjesterkrook.uftb` | King+Jester vs King+Rook | 571,417,768 | 5,584,592 / 1,291,354 / 12,103,014 | 10,758,014 / 4,634 / 8,216,312 | `ba1a6ecd7e71b6e3fbe1bff9a1576c0d4a9d0cc2f241aefd48ea519959ef984d` |
| `kjesterbishopk.uftb` | King+Jester+Bishop vs King | 504,293,320 | 18,978,960 / 0 / 0 | 1,609,608 / 16,113,394 / 1,255,958 | `5816410c814d49a27dac924a0f5beb1b792b61b742a2fc83f0411bf10d15eae0` |
| `kjesterkbishop.uftb` | King+Jester vs King+Bishop | 489,530,528 | 5,600,180 / 0 / 13,378,780 | 3,693,788 / 10,706 / 15,274,466 | `698f617b02ec063f481f57dac1b3677e7d9db618ca006dac59a3105c872dfd43` |
| `kjesterbombk.uftb` | King+Jester+Bomb vs King | 512,779,944 | 18,978,960 / 0 / 0 | 1,609,608 / 17,331,368 / 37,984 | `5fdb70947be34ad506f893291c4657a4b0f850e39672caae6e564a1df71486fb` |
| `kjesterkbomb.uftb` | King+Jester vs King+Bomb | 494,687,920 | 3,248,390 / 9,716,844 / 6,013,726 | 15,947,500 / 29,024 / 3,002,436 | `b01b14d0a5d920e9c519c1c9a310882c7aec263c8a7aa71c2abbefb3202dc8cb` |
| `kjesterninjak.uftb` | King+Jester+Ninja vs King | 629,742,840 | 18,978,960 / 0 / 0 | 1,609,608 / 17,313,736 / 55,616 | `33431727e988e9c2f48dc0f22e3418072fc8c6f741290b037fd1a5e28b0bbf97` |
| `kjesterkninja.uftb` | King+Jester vs King+Ninja | 602,269,760 | 5,592,936 / 6,238,816 / 7,147,208 | 15,125,298 / 5,680 / 3,847,982 | `7255da99637afb9a015c284056b4be3431d602c6c0febddb7fd439157170a55b` |
| `kjesterturtlek.uftb` | King+Jester+Turtle vs King | 409,044,272 | 18,978,960 / 0 / 0 | 1,609,608 / 16,006,830 / 1,362,522 | `496ab5eb6012ff5500b54b6589d153710839563621cbb4340e6f69534a0d3067` |
| `kjesterkturtle.uftb` | King+Jester vs King+Turtle | 402,759,128 | 11,993,228 / 0 / 6,985,732 | 2,397,164 / 5,356,036 / 11,225,760 | `7216f1e140c32fc3ed69addce02b651f64ec3fef5173f7b0c11259fcc38ccebd` |
| `kjestermagek.uftb` | King+Jester+Mage vs King | 386,478,416 | 18,978,960 / 0 / 0 | 1,609,608 / 15,952,244 / 1,417,108 | `9d48c76c568315e9ff533fe3d518f1c7fe528c421891b65b1e171657f2ed7c09` |
| `kjesterkmage.uftb` | King+Jester vs King+Mage | 364,406,212 | 18,978,960 / 0 / 0 | 1,609,608 / 15,953,304 / 1,416,048 | `705b00311fc61e20615b79e13472b96cf7705f8e80227303877ec0445326d274` |
| `kjesterparasitek.uftb` | King+Jester+Parasite vs King | 462,590,064 | 18,978,960 / 0 / 0 | 1,609,608 / 17,365,128 / 4,224 | `50d5e2a3a35f7746b27395ebeb922dd9583b10800b59a6870c18750416ac5630` |
| `kjesterkparasite.uftb` | King+Jester vs King+Parasite | 450,905,824 | 3,349,672 / 15,592,716 / 36,572 | 18,877,766 / 87,668 / 13,526 | `45f0f26f182a2e231455df61931cf49d6a54d9b36f9be0e1fd9fd87117a7fd18` |
| `kjestergiantk.uftb` | King+Jester+Giant vs King | 259,948,596 | 13,286,700 / 0 / 5,692,260 | 1,142,116 / 11,300,932 / 6,535,912 | `9cee141bd1bc8cd2e0427a95d6b96683e24e5096d2bafce7390750918a5eb1f8` |
| `kjesterkgiant.uftb` | King+Jester vs King+Giant | 265,285,348 | 13,180,164 / 21,472 / 5,777,324 | 3,091,428 / 7,778,226 / 8,109,306 | `67a539cfcbeeb53837a6b7c07c497aed513aa9d12260a7588213eee783a204e6` |
| `kknightknightk.uftb` | King+2 Knights vs King | 196,460,680 | 1,964,822 / 0 / 7,524,658 | 0 (804,804) / 68 / 8,684,608 | `5c95ba0ed74d95e4d2c2fa4d1a98c1dddb121a9d11406853c75d4d5e1ba15138` |
| `kknightturtlek.uftb` | King+Knight+Turtle vs King | 363,952,568 | 17,632,308 / 0 / 1,346,652 | 0 (1,609,608) / 12,261,896 / 5,107,456 | `e51001e72d79078c3da96de0dab87d8752d5f7496621b4b3812f82624107c5df` |
| `kbishopbishopk.uftb` | King+2 Bishops vs King | 130,467,458 | 4,804,466 / 0 / 4,685,014 | 0 (407,502) / 3,708,210 / 5,373,768 | `18ee411f10c1c62e365b9e30359a1403f5d34e827bcc5509f20d8c0e73ded490` |
| `kbombbombk.uftb` | King+2 Bombs vs King | 260,111,166 | 9,489,402 / 78 / 0 | 0 (804,804) / 8,652,882 / 31,794 | `3007861257e32343194410f89e6c446a904086162f15481d46d7a5337404a4ab` |
| `kturtleturtlek.uftb` | King+2 Turtles vs King | 167,494,620 | 1,579,316 / 0 / 7,910,164 | 0 (804,804) / 416 / 8,684,260 | `bbb146a2f252eaf0eb40d23e49d0ddda56de183fb202c515ff64a0fd172037b1` |
<!-- GENERATED_TABLE_END -->

Each W / L / D cell is from the perspective of the side to move named by its
column. Parentheses report illegal adjacent-King states separately and exclude
them from the preceding legal count. A live Jester intentionally permits its
real King to remain threatened, so the 41,808 bare-King-side wins in
`kjesterk` are legal and are not parenthesized.

Bundled files use packed format version 4 (or v5 when a character has a second
linked/material model): a two-bit WDL plane, one-byte DTW
plane, and a sparse exact exception stream for distances of 255 or more. This
is 37.5% smaller than the former 16-bit records before entropy coding. Every file
was validated after retrograde propagation by
regenerating all legal successors and checking the Bellman equations for every
state. Giant anchor tuples whose 2x2 footprint is off-board or overlaps a King
are explicit draw sentinels and can never be produced by the probe.

The state dimensions preserve Ghost visibility, Sniper cooldown, both Prince
action phases, Pawn moved state, and the Penguin's cooldown plus exact reachable
freeze aura. Pawn promotions cross-probe the exact Queen table. A double-step
en-passant marker is quotient-equivalent here because no opposing Pawn exists
to use it.
Copycat is one deployable character with two board models; in this material
class its clone remains the exact horizontal mirror of the selected half, so
the invariant is encoded directly rather than storing unreachable arbitrary
clone coordinates.

`tools/plan_ultimate_tablebases.py` is the authoritative class and storage
inventory for the expansion. It applies horizontal-reflection canonicalization
and budgets separate two-bit WDL and byte DTW planes. Future large logical
files are split into SHA-256-checked parts of at most 95 MB, below GitHub's
regular 100 MB per-file limit; the engine materializes them transparently and
the repository does not use Git LFS. The planner treats entropy compression as
extra margin,
not as a speculative assumption when enforcing the 10 GiB total cap.

Regenerate them with `tools/generate_ultimate_tablebases.sh`. Checkpoints are
piece-tagged and resumable. Lone Bishop, Knight, Turtle, Mage, Devil, Sludge,
Checker, Angel, and Fisherman classes are not bundled because the recovered
native insufficient-material rule makes each an immediate draw.

Of the 182 possible stateless K+K+2 material classes, 160 are bundled and the
remaining 22 are omitted as immediate insufficient-material draws. The 22
omitted classes are King+Knight+Mage, King+Knight+Fisherman, King+Turtle+Mage,
King+Turtle+Fisherman, King+Mage+Mage, King+Mage+Fisherman, and
King+Fisherman+Fisherman versus a bare King, plus King+Knight versus
King+Knight, Bishop, Turtle, Mage, or Fisherman; King+Bishop versus
King+Bishop, Turtle, Mage, or Fisherman; King+Turtle versus King+Turtle, Mage,
or Fisherman; King+Mage versus King+Mage or Fisherman; and King+Fisherman
versus King+Fisherman.

The 10 GiB budget additionally admits 24 stateful classes: K+Bomb+Ghost,
K+Bomb+Prince, K+Pawn+Bomb, K+Bomb+Checker, K+Bomb+Sniper,
K+Berserker+Bomb, K+Bomb+Penguin, K+Bishop+Ghost, K+Bishop versus K+Ghost,
K+Bishop versus K+Prince, K+Bishop+Prince, K+Bomb versus K+Ghost, K+Bomb
versus K+Prince, K+Ghost+Dragon, K+Ghost+Fisherman, K+Ghost+Ghost,
K+Ghost+Giant, K+Ghost versus K+Dragon, K+Ghost versus K+Fisherman, K+Ghost
versus K+Giant, K+Ghost versus K+Mage, K+Ghost versus K+Parasite,
K+Ghost+Mage, and K+Ghost+Parasite. Devil, Sludge, Angel, and Copycat
combinations are excluded from K+K+2 because Minion spawning, persistent
Goop, Angel host/Halo state, and Copycat's linked clone make those classes
larger than the exact four-model closure used here; representing them as
ordinary K+K+2 tables would be incorrect.
