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
| `kjesterfishermank.uftb` | King+Jester+Fisherman vs King | 796,241,336 | 18,978,960 / 0 / 0 | 1,609,608 / 15,952,244 / 1,417,108 | `3f335332d457d184fc3a20b5ddddf2806911bb5e5bfd607872eb757a2a90a968` |
| `kjesterkfisherman.uftb` | King+Jester vs King+Fisherman | 723,303,740 | 5,592,998 / 0 / 13,385,962 | 1,609,608 / 11,842 / 17,357,510 | `ce378df3a2be9b6fe0dcb82098a88abc7067ed1cde02821ca744af46dc461df0` |
| `kjesterdragonk.uftb` | King+Jester+Dragon vs King | 596,943,520 | 18,978,960 / 0 / 0 | 1,609,608 / 17,350,088 / 19,264 | `3553fad7d75c3c428bbf0d9333801d57cb3dae0a99bc505ef1bf45de1f7cc017` |
| `kjesterkdragon.uftb` | King+Jester vs King+Dragon | 573,650,240 | 5,587,240 / 7,153,948 / 6,237,772 | 16,036,060 / 5,426 / 2,937,474 | `55477de8dd1263ab734582dea3409af5409fa704bf35088d4ab4300beee7a5fb` |
| `kknightknightk.uftb` | King+2 Knights vs King | 196,460,680 | 1,964,822 / 0 / 7,524,658 | 0 (804,804) / 68 / 8,684,608 | `5c95ba0ed74d95e4d2c2fa4d1a98c1dddb121a9d11406853c75d4d5e1ba15138` |
| `kknightqueenk.uftb` | King+Knight+Queen vs King | 674,814,946 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 15,991,680 / 1,377,672 | `5a2143cee6221564e0acaf53e5d0f5fd764f2997445fc8bbc490deb79c5693ba` |
| `kknightkqueen.uftb` | King+Knight vs King+Queen | 616,478,182 | 2,808,960 / 13,559,860 / 2,610,140 | 18,924,172 / 0 / 54,788 | `c708a791064b0b9f45bd506b3cfc1657912890dd1fb080533d311db6c3418750` |
| `kknightrookk.uftb` | King+Knight+Rook vs King | 533,160,532 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 16,041,948 / 1,327,404 | `04513bc582ac66c262ce0a50aadc7dc4a103cf281b5ae66aa06a73fd4a78d457` |
| `kknightkrook.uftb` | King+Knight vs King+Rook | 497,289,662 | 2,808,976 / 1,871,092 / 14,298,892 | 11,603,962 / 4 / 7,374,994 | `4f1655eb31c7a335de951bcc217a2e01b777872b2fc867fead6d42ed3f255da8` |
| `kknightbishopk.uftb` | King+Knight+Bishop vs King | 450,616,302 | 18,933,414 / 0 / 45,546 | 0 (1,609,608) / 14,751,040 / 2,618,312 | `e78a30ba8605cccbbc3c9198dd717843ef80115905b7ea05b03f130756d54f48` |
| `kknightbombk.uftb` | King+Knight+Bomb vs King | 457,027,800 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,337,540 / 31,812 | `6c1e33d2b92fe411485a2370fb18621c86d5e26de45a3deb053f0a4547312d72` |
| `kknightkbomb.uftb` | King+Knight vs King+Bomb | 431,449,080 | 2,903,304 / 14,810,768 / 1,264,888 | 18,908,960 / 52 / 69,948 | `64f63059b0d87f4b28bf8d30c0a1c3354ed42441fd6b84c6a1be26263aa9afde` |
| `kknightninjak.uftb` | King+Knight+Ninja vs King | 564,591,464 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 16,000,796 / 1,368,556 | `ecb39a6f313de06083df9552b6b46e291fdfda4f04c4536744096eea5615a3fa` |
| `kknightkninja.uftb` | King+Knight vs King+Ninja | 524,358,488 | 2,808,960 / 13,522,364 / 2,647,636 | 18,916,224 / 0 / 62,736 | `c72d856b445ae577e4394e9b0fe4ec3589a3c0fd55bd991d037a709bad688e78` |
| `kknightturtlek.uftb` | King+Knight+Turtle vs King | 363,952,568 | 17,632,308 / 0 / 1,346,652 | 0 (1,609,608) / 12,261,896 / 5,107,456 | `e51001e72d79078c3da96de0dab87d8752d5f7496621b4b3812f82624107c5df` |
| `kknightparasitek.uftb` | King+Knight+Parasite vs King | 412,737,272 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,364,220 / 5,132 | `80703fb82f8caf5589e4e3d07bec5f6b4ce78dec12ce44d7a7f7e07d5e127246` |
| `kknightkparasite.uftb` | King+Knight vs King+Parasite | 396,499,600 | 2,808,980 / 14,791,096 / 1,378,884 | 18,910,940 / 4 / 68,016 | `c9c65bc38098b46e30464eabc4816c9d4e68c498b0876c7dca2df59103257c04` |
| `kknightgiantk.uftb` | King+Knight+Giant vs King | 228,702,604 | 13,080,248 / 0 / 5,898,712 | 0 (1,142,116) / 9,132,192 / 8,704,652 | `2aecb1ceed4793a173c917fda61cb26bd622bf4dfec8e66d61c0e837f5cba541` |
| `kknightkgiant.uftb` | King+Knight vs King+Giant | 215,826,804 | 1,968,832 / 19,388 / 16,990,740 | 3,089,504 / 0 / 15,889,456 | `c3c8ceb56c11bbd52c3316e8dbb786e4fbe573382b6190b0e2596bf49ffda8fc` |
| `kknightdragonk.uftb` | King+Knight+Dragon vs King | 534,668,038 | 18,978,956 / 0 / 4 | 0 (1,609,608) / 16,039,998 / 1,329,354 | `5a186985b6ca6e024562e2a0cff6dbe34003e30903761bae751400135abeb01c` |
| `kknightkdragon.uftb` | King+Knight vs King+Dragon | 498,929,176 | 2,808,960 / 13,805,686 / 2,364,314 | 18,934,576 / 0 / 44,384 | `22a14009f6d4f456eddcb659d3a5c1d99170ff4c706a517b786357abaecddabb` |
| `kqueenqueenk.uftb` | King+2 Queens vs King | 480,508,116 | 9,489,480 / 0 / 0 | 0 (804,804) / 8,560,812 / 123,864 | `953ef23458f3681680b5d58736d854930218e59dea4961d8ddec5632108f1b21` |
| `kqueenkqueen.uftb` | King+Queen vs King+Queen | 701,876,100 | 11,654,618 / 47,930 / 7,276,412 | 11,654,618 / 47,930 / 7,276,412 | `41421585773f70182648370f85b441f6bb7ad39a43a1d9926b249a3c4f435fe1` |
| `kqueenrookk.uftb` | King+Queen+Rook vs King | 816,632,332 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,251,240 / 118,112 | `8674db75197f632ee8a9a0740b3569a49082929879fbd79ea70fe30efd10eaea` |
| `kqueenkrook.uftb` | King+Queen vs King+Rook | 656,954,204 | 18,882,984 / 24,998 / 70,978 | 8,648,160 / 9,667,734 / 663,066 | `e3616069bee07342fda4eed31660b8504ca8c0ac149038c8bbc35e10beb89a90` |
| `kqueenbishopk.uftb` | King+Queen+Bishop vs King | 732,953,056 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 16,034,380 / 1,334,972 | `8ce01962e4b7dde87d17de918bfe89ce50337bd353de2cd19ec8c2b99739774e` |
| `kqueenkbishop.uftb` | King+Queen vs King+Bishop | 633,491,052 | 18,951,442 / 0 / 27,518 | 3,693,788 / 12,200,676 / 3,084,496 | `3cb84ad6176d504554d46a67b62a03853d8590e6940ed9d66a6e983e0e2594f6` |
| `kqueenbombk.uftb` | King+Queen+Bomb vs King | 738,060,636 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,268,888 / 100,464 | `a75bcbfedb3e5fc4f35d711e81b50db0b23d93ef232bdf1ee5b15c44787cf5d9` |
| `kqueenkbomb.uftb` | King+Queen vs King+Bomb | 629,403,948 | 8,774,424 / 1,328,334 / 8,876,202 | 7,454,474 / 671,100 / 10,853,386 | `7317a875224cf62e25de4b51c3bcabb3720fce8c0ebdab45e1db2cd2f4db9330` |
| `kqueenninjak.uftb` | King+Queen+Ninja vs King | 849,027,988 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,178,972 / 190,380 | `d9b0d5151b48d5137566ed0a0d7364af0122b1b68adfcc6d946afc9a31881f74` |
| `kqueenkninja.uftb` | King+Queen vs King+Ninja | 672,276,532 | 14,565,046 / 9,456 / 4,404,458 | 8,881,656 / 1,365,734 / 8,731,570 | `d406e996eabed529e140e6e31c93a5841561e412ce0350f8e36d169f0ddc8e4c` |
| `kqueenturtlek.uftb` | King+Queen+Turtle vs King | 644,511,080 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 15,954,050 / 1,415,302 | `c2a325eccaf941235c632d386beac95de4cdcc22b2005511b80e538f3353f39a` |
| `kqueenkturtle.uftb` | King+Queen vs King+Turtle | 606,531,770 | 18,978,928 / 0 / 32 | 2,397,164 / 14,534,402 / 2,047,394 | `353627128552df82c4eece2999e8e6805a11b29a418078c7f4f255f5e0ec55fe` |
| `kqueenmagek.uftb` | King+Queen+Mage vs King | 623,307,860 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 15,912,942 / 1,456,410 | `de3e8f1a0b0d1d5f216c8be8d4ab9efccf8a7a09a4c803175a4e6651c451a22e` |
| `kqueenkmage.uftb` | King+Queen vs King+Mage | 600,512,500 | 18,978,960 / 0 / 0 | 1,609,608 / 15,938,592 / 1,430,760 | `88a74961c77c7da83eded1c1d716037ffbe71a46746849651c3a308141d4b2f4` |
| `kqueenparasitek.uftb` | King+Queen+Parasite vs King | 694,110,316 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,307,456 / 61,896 | `6a29b00ab2a1edd510e94e67d5ca70d6da108b15bc9d9a3edd54b547d2b8327c` |
| `kqueenkparasite.uftb` | King+Queen vs King+Parasite | 622,639,914 | 9,388,712 / 794,236 / 8,796,012 | 6,833,856 / 1,561,098 / 10,584,006 | `f088d7d7937b6efed0cc69eec38fda1448bbb0351d135b9fb07027a9ad578d39` |
| `kqueengiantk.uftb` | King+Queen+Giant vs King | 411,446,456 | 13,286,700 / 0 / 5,692,260 | 0 (1,142,116) / 11,256,584 / 6,580,260 | `b1e6c719cd0a03774e09c3bb700b63e54f1b6ad940780498c0d5ae3169302d85` |
| `kqueenkgiant.uftb` | King+Queen vs King+Giant | 358,244,540 | 13,273,492 / 7,090 / 5,698,378 | 3,072,430 / 7,900,780 / 8,005,750 | `e9ce180e7176f25be6857375977f5c4f56fc5b5b5805fa1c60a22e6a15510baa` |
| `kqueenfishermank.uftb` | King+Queen+Fisherman vs King | 998,335,056 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 15,912,942 / 1,456,410 | `d8999ba65bae1875637e5cc4e5b7ad857bb178533a7f41751cbdadce731f1c38` |
| `kqueenkfisherman.uftb` | King+Queen vs King+Fisherman | 870,038,166 | 18,978,960 / 0 / 0 | 1,609,608 / 15,953,468 / 1,415,884 | `c321eebe57da1b0b119a42001f06b98d1353528567fc9c6dffcd09b95c27e3b2` |
| `kqueendragonk.uftb` | King+Queen+Dragon vs King | 819,198,846 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,238,014 / 131,338 | `59dbc00aeff1770fdae48bb5ebeb6db85c4318e17d95f1fa531cc4fcf4c9f32e` |
| `kqueenkdragon.uftb` | King+Queen vs King+Dragon | 661,400,078 | 15,001,422 / 108,042 / 3,869,496 | 9,183,222 / 1,437,880 / 8,357,858 | `c4cf5caf8667b03528602cfa24f1b35e13bd1d038c5791fb028196965988d7cd` |
| `krookrookk.uftb` | King+2 Rooks vs King | 336,683,762 | 9,489,480 / 0 / 0 | 0 (804,804) / 8,669,238 / 15,438 | `6ad45d2181b8bb5e7780e8bb87b654970778d108405c43d21c06954ed287a635` |
| `krookkrook.uftb` | King+Rook vs King+Rook | 574,799,708 | 8,699,932 / 85,386 / 10,193,642 | 8,699,932 / 85,386 / 10,193,642 | `74424fbc58c6b6e837780bed913ada2eb256cdcaf27fb53162c4b84e1e9d763e` |
| `krookbishopk.uftb` | King+Rook+Bishop vs King | 591,300,396 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 16,100,204 / 1,269,148 | `0c5930e4043e0581648739b7d5fc499ce18daec8d18e19f04836f27ca49cc2ad` |
| `krookkbishop.uftb` | King+Rook vs King+Bishop | 530,190,084 | 9,534,390 / 0 / 9,444,570 | 3,693,788 / 423,770 / 14,861,402 | `ce2f769cedcf748f0cf45fb78eb7f642c5df51e01f2aad7263a65238ab82bc56` |
| `krookbombk.uftb` | King+Rook+Bomb vs King | 596,543,578 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,310,732 / 58,620 | `6af34f7ec80a32ea17a32a38a3ce217e620a3883a5d637e7077706c1b5260443` |
| `krookkbomb.uftb` | King+Rook vs King+Bomb | 529,230,264 | 5,267,980 / 2,871,312 / 10,839,668 | 9,989,536 / 15,114 / 8,974,310 | `8c427bef832972b822da38aedd9c2acbb86fe8f4e1a615c84644cf0e729f5121` |
| `krookninjak.uftb` | King+Rook+Ninja vs King | 705,763,068 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,271,872 / 97,480 | `a19e735be310cbf8caa6e8397d8202d134c16a51f9f5de486c3f4f56583584ea` |
| `krookkninja.uftb` | King+Rook vs King+Ninja | 597,070,432 | 8,785,322 / 275,140 / 9,918,498 | 10,001,602 / 131,520 / 8,845,838 | `f3b3c7caa225f800a73cdb302dec38601f02bd805700a1cbb001b41844b23492` |
| `krookturtlek.uftb` | King+Rook+Turtle vs King | 503,391,432 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 16,000,700 / 1,368,652 | `e2752a01a8c10a08344f8aa51373a2913a5c0175b48a476baccb907e7fc77526` |
| `krookkturtle.uftb` | King+Rook vs King+Turtle | 480,007,102 | 18,955,240 / 4 / 23,716 | 2,397,168 / 14,534,220 / 2,047,572 | `9250b5addb8e8b775422b8eb5d562337847f03ea08fe21cc660e2b7849045819` |
| `krookmagek.uftb` | King+Rook+Mage vs King | 482,774,292 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 15,950,608 / 1,418,744 | `b1704edd22b133cf0e3b036a69990353ce923d10f524aa3802d2ce54d03326ca` |
| `krookkmage.uftb` | King+Rook vs King+Mage | 462,063,112 | 18,978,960 / 0 / 0 | 1,609,608 / 15,952,876 / 1,416,476 | `f7f3e7b23902188d6c0e47673f8c59b483656de94486fa52a0affcb6ced4abd7` |
| `krookparasitek.uftb` | King+Rook+Parasite vs King | 552,591,676 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,360,968 / 8,384 | `91ca9a8f1a35291a384b84a2813e29433b331cb354039cd992d860cad7ac3c19` |
| `krookkparasite.uftb` | King+Rook vs King+Parasite | 508,640,722 | 5,052,318 / 13,479,612 / 447,030 | 18,808,556 / 17,690 / 152,714 | `05d30182c25ad00379ab4e5c1d51223cb108ee133c30d00efc89faa012e9aed2` |
| `krookgiantk.uftb` | King+Rook+Giant vs King | 321,003,062 | 13,286,700 / 0 / 5,692,260 | 0 (1,142,116) / 11,296,414 / 6,540,430 | `36b212cd1ac7cc12ad2773453e30d9baef52b691488e5ffe777698708b8d0bc6` |
| `krookkgiant.uftb` | King+Rook vs King+Giant | 287,592,138 | 13,212,128 / 13,856 / 5,752,976 | 3,082,334 / 7,888,652 / 8,007,974 | `82ef14d4804794e0aba2a3d0faec6441e261f5200546f726f28e50bc1bbc72d6` |
| `krookfishermank.uftb` | King+Rook+Fisherman vs King | 857,801,488 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 15,950,608 / 1,418,744 | `093944252b6b71ad34c31a006493a6bb6ab46fef02238e6f1d4714dacf0e6707` |
| `krookkfisherman.uftb` | King+Rook vs King+Fisherman | 778,458,154 | 9,374,180 / 0 / 9,604,780 | 1,609,608 / 461,572 / 16,907,780 | `bd7c952e4779e7b9fd1e035dc1e9eabcd2202c51f0e96b0dfbe38bdd20b713a9` |
| `krookdragonk.uftb` | King+Rook+Dragon vs King | 676,425,340 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,325,054 / 44,298 | `7705efb335912c0ed7c8f7dd7c1ac63ea6cce8f353ce5f8f4799d8a09aa4f70c` |
| `krookkdragon.uftb` | King+Rook vs King+Dragon | 579,444,158 | 9,327,596 / 437,638 / 9,213,726 | 10,675,404 / 324,630 / 7,978,926 | `59d1e0f5d1a560efbd24fde36c9d5d6947e64ec50509b5500b56cff1b28644d8` |
| `kbishopbishopk.uftb` | King+2 Bishops vs King | 130,467,458 | 4,804,466 / 0 / 4,685,014 | 0 (407,502) / 3,708,210 / 5,373,768 | `18ee411f10c1c62e365b9e30359a1403f5d34e827bcc5509f20d8c0e73ded490` |
| `kbishopbombk.uftb` | King+Bishop+Bomb vs King | 514,356,290 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,333,560 / 35,792 | `7551d93b34162827fa0b35578b4c6a4d9acd181bddcaa31582ab1361756764b1` |
| `kbishopkbomb.uftb` | King+Bishop vs King+Bomb | 471,646,156 | 3,850,636 / 13,199,096 / 1,929,228 | 18,829,288 / 40 / 149,632 | `8f3edd16e534811b61a4dcd792f2bb7a26e7c6f43fb0397c9c3696de56d0ddbe` |
| `kbishopninjak.uftb` | King+Bishop+Ninja vs King | 622,467,416 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 16,048,930 / 1,320,422 | `25afeb53b8ab30386bf683f20c1769220d7e3b1c1356e543c60f1f260ad58985` |
| `kbishopkninja.uftb` | King+Bishop vs King+Ninja | 554,408,596 | 3,693,788 / 12,195,462 / 3,089,710 | 18,939,060 / 0 / 39,900 | `cd948abb01bd00c51ffdf0c4aebb46394b4f3052e6b86073c8b4e327fa813f22` |
| `kbishopturtlek.uftb` | King+Bishop+Turtle vs King | 421,326,784 | 18,936,088 / 0 / 42,872 | 0 (1,609,608) / 14,694,114 / 2,675,238 | `356dc50f942ccd939ea3a1364b4481691b4abf9bb4338c65fef015b567bea827` |
| `kbishopmagek.uftb` | King+Bishop+Mage vs King | 400,784,944 | 3,693,788 / 0 / 15,285,172 | 0 (1,609,608) / 0 / 17,369,352 | `4b32c84e4e56475370f5439dcd28b75a743e86531c8f27c4b652d47b722b767a` |
| `kbishopparasitek.uftb` | King+Bishop+Parasite vs King | 470,086,096 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,355,020 / 14,332 | `f47108fdea572a895b49d0e1ac7f94e342f0029a753a7aa3cd99217ae4d275fb` |
| `kbishopkparasite.uftb` | King+Bishop vs King+Parasite | 442,566,648 | 3,693,788 / 13,243,670 / 2,041,502 | 18,831,764 / 0 / 147,196 | `4974a88a0665613f8097467df8bf39c67a1048046963e7be6f0cbbd802e879b0` |
| `kbishopgiantk.uftb` | King+Bishop+Giant vs King | 263,060,894 | 9,818,744 / 0 / 9,160,216 | 0 (1,142,116) / 5,650,886 / 12,185,958 | `f7bed858352d77238adb64d894d2e2ce4bc716703aa4354fa16f288708bd658c` |
| `kbishopkgiant.uftb` | King+Bishop vs King+Giant | 242,856,394 | 2,519,718 / 15,910 / 16,443,332 | 3,084,448 / 0 / 15,894,512 | `0693144e84471959edad34705a14dbef0685df3b60bc6d83960b904001133bb2` |
| `kbishopfishermank.uftb` | King+Bishop+Fisherman vs King | 775,812,140 | 3,693,788 / 0 / 15,285,172 | 0 (1,609,608) / 0 / 17,369,352 | `3e2b9df328f5ae439a9d4559b83e5d7d80ae35c07004c9f3f6abccff7cc254f3` |
| `kbishopdragonk.uftb` | King+Bishop+Dragon vs King | 592,268,962 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 16,094,422 / 1,274,930 | `e5441c998ebc7f4ef1a7be2079a7919b96a2e02238a21ccd4b0e342925bfc6fd` |
| `kbishopkdragon.uftb` | King+Bishop vs King+Dragon | 531,451,376 | 3,693,796 / 12,191,210 / 3,093,954 | 18,874,392 / 4 / 104,564 | `8c0fba9b456b74d6784bd9b48932a1ccabf44237792ee57413bfbf55a95d2be0` |
| `kbombbombk.uftb` | King+2 Bombs vs King | 260,111,166 | 9,489,402 / 78 / 0 | 0 (804,804) / 8,652,882 / 31,794 | `3007861257e32343194410f89e6c446a904086162f15481d46d7a5337404a4ab` |
| `kbombkbomb.uftb` | King+Bomb vs King+Bomb | 477,943,752 | 10,281,688 / 4,806,636 / 3,890,636 | 10,281,688 / 4,806,636 / 3,890,636 | `c32ed66d65ba864590c1539a383f2e74d703d9e0b7a221226bf91bde30816736` |
| `kbombninjak.uftb` | King+Bomb+Ninja vs King | 628,229,260 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,287,464 / 81,888 | `6b3ae1d52b88034772c3ecc8d59303dfe2b66c23f199d70b1613060f8f0017f6` |
| `kbombkninja.uftb` | King+Bomb vs King+Ninja | 554,146,022 | 8,743,756 / 489,670 / 9,745,534 | 6,611,870 / 2,230,988 / 10,136,102 | `3c4bbaa19cd78e42cc8d5f74c14ba98052ae381c0b705123357a11fd364ca9d5` |
| `kbombturtlek.uftb` | King+Bomb+Turtle vs King | 427,769,788 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,337,600 / 31,752 | `ee4dca71935ad3215f55f9647542ca737f86a6b092ef36923ae7662032b827de` |
| `kbombkturtle.uftb` | King+Bomb vs King+Turtle | 411,556,236 | 18,978,596 / 4 / 360 | 2,437,986 / 15,886,854 / 654,120 | `b3ed8e16f550dc7325b87bf02578d8b949f758f070a7c7e98cddfd741c107f91` |
| `kbombmagek.uftb` | King+Bomb+Mage vs King | 407,442,416 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,339,240 / 30,112 | `17d8e361f33ffa0b7e2ea366d9a68711ea96a2115aab8fd4de09b55800f412af` |
| `kbombkmage.uftb` | King+Bomb vs King+Mage | 387,466,920 | 18,978,960 / 0 / 0 | 1,609,608 / 17,363,848 / 5,504 | `a7469c100c45ef088a63fb7016ea648a2b93e8ed3793054d83961f364e92fbe3` |
| `kbombparasitek.uftb` | King+Bomb+Parasite vs King | 476,419,868 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,331,368 / 37,984 | `2af30741c6d6e9892aac5bd68b397454f3e351f0e459d04c274e47a7f80d9a30` |
| `kbombkparasite.uftb` | King+Bomb vs King+Parasite | 447,255,924 | 10,818,954 / 3,010,424 / 5,149,582 | 8,467,632 / 5,296,948 / 5,214,380 | `bc300fdbcb2945fd8fe494f09337a39f28fe7f6c41debac8535a7b124fca3d5f` |
| `kbombgiantk.uftb` | King+Bomb+Giant vs King | 273,239,456 | 13,286,700 / 0 / 5,692,260 | 0 (1,142,116) / 12,116,408 / 5,720,436 | `50ad69316185c89f66f3f15d61ce3f6f969419eea73b5dbd331cc6d831cbae5e` |
| `kbombkgiant.uftb` | King+Bomb vs King+Giant | 249,537,360 | 12,522,720 / 19,520 / 6,436,720 | 3,152,398 / 7,689,034 / 8,137,528 | `b504391775e091cd8ca49bca0763e9359262ad794a7f9b6cd79d80d66a6fb9f1` |
| `kbombfishermank.uftb` | King+Bomb+Fisherman vs King | 781,013,976 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,339,240 / 30,112 | `c35a204b36baf78a0c823f857b370adeb4a22a2aa1b7eb159cface00da7e5b6b` |
| `kbombkfisherman.uftb` | King+Bomb vs King+Fisherman | 729,381,942 | 7,728,782 / 0 / 11,250,178 | 1,609,608 / 1,511,094 / 15,858,258 | `afdd6e53d8f69c864f9c4dd29d44082319d434640b52157181cf078a31ddbf72` |
| `kbombdragonk.uftb` | King+Bomb+Dragon vs King | 598,544,858 | 18,978,960 / 0 / 0 | 0 (1,609,608) / 17,324,550 / 44,802 | `062163e51a367a6558d6cf520594ea40fcf3ba1f4d1572ea4a8eb62bcbd2f9ff` |
| `kbombkdragon.uftb` | King+Bomb vs King+Dragon | 531,583,888 | 8,286,332 / 116,520 / 10,576,108 | 5,598,358 / 1,946,092 / 11,434,510 | `6b1c65f252af9e836c6ccf51a368baaca815bffeeb2d1efca8034cf8c85a900e` |
| `kninjaninjak.uftb` | King+2 Ninjas vs King | 368,966,508 | 9,489,480 / 0 / 0 | 0 (804,804) / 8,609,904 / 74,772 | `18fd0757d22df706de52658fc16ff2448be7ddb042e4c84b5561983c1031d6c8` |
| `kturtleturtlek.uftb` | King+2 Turtles vs King | 167,494,620 | 1,579,316 / 0 / 7,910,164 | 0 (804,804) / 416 / 8,684,260 | `bbb146a2f252eaf0eb40d23e49d0ddda56de183fb202c515ff64a0fd172037b1` |
| `kmagekparasite.uftb` | King+Mage vs King+Parasite | 344,453,172 | 1,609,608 / 17,369,180 / 172 | 18,978,960 / 0 / 0 | `7d8638ada4d6e2fb5314afc7b87bf25a4013f149b76525f44ba561a55cf147aa` |
| `kfishermankparasite.uftb` | King+Fisherman vs King+Parasite | 703,350,700 | 1,609,608 / 314,868 / 17,054,484 | 4,835,644 / 0 / 14,143,316 | `6032d4e60931298ce7fc31341cde1318a4fdcd19604c29a1a260a577d0f4c389` |
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
