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
| `kjesterk.uftb` | King+Jester vs King | 9,169,752 | 412,616 (80,344) / 0 / 0 | 41,808 / 414,344 / 36,808 | `3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa` |
| `kpawnk.uftb` | King+Pawn vs King | 12,759,470 | 576,806 (101,696) / 0 / 217,258 (90,160) | 0 (83,616) / 459,272 / 352,872 (90,160) | `42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844` |
| `kqk.uftb` | King+Queen vs King | 15,744,492 | 306,404 (186,556) / 0 / 0 | 0 (41,808) / 413,304 / 37,848 | `1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3` |
| `krk.uftb` | King+Rook vs King | 11,970,912 | 361,648 (131,312) / 0 / 0 | 0 (41,808) / 414,300 / 36,852 | `abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44` |
| `kberserkerk.uftb` | King+Berserker vs King | 92,321,028 | 1,468,376 (3,461,224) / 0 / 0 | 0 (418,080) / 4,143,200 / 368,320 | `f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1` |
| `kbombk.uftb` | King+Bomb vs King | 9,761,788 | 394,988 (97,972) / 0 / 0 | 0 (41,808) / 448,600 (1,760) / 792 | `3d4f44035652e486cbd72c59e8247cfbba355b748107ee2b9d06abfe4674b864` |
| `kninjak.uftb` | King+Ninja vs King | 12,610,592 | 356,360 (136,600) / 0 / 0 | 0 (41,808) / 413,376 / 37,776 | `4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776` |
| `kghostk.uftb` | King+Ghost vs King | 17,761,144 | 863,768 (122,152) / 0 / 0 | 0 (83,616) / 826,992 (38,520) / 36,776 (16) | `3be39c5ab2bfec00cb9dd500e26911bd145bcb1f4dde77fd2c84ef33d111fc31` |
| `kpenguink.uftb` | King+Penguin vs King | 9,874,656 | 384 (48,064) / 192 / 489,112 (448,168) | 796 (43,568) / 1,760 / 527,180 (412,616) | `7097ade2d86569ab5d5edf5b5700eaf0c5fa576f92d3510f523b2bf4625b7cbd` |
| `kparasitek.uftb` | King+Parasite vs King | 8,602,716 | 412,616 (80,344) / 0 / 0 | 0 (41,808) / 451,120 / 32 | `f08b2676a703259ef638bc4ab72b9cd7e6b2000afc72dd70b0ac17e03ea858ab` |
| `ksniperk.uftb` | King+Sniper vs King | 24,180,908 | 5,014 (194,552) / 0 / 872,222 (900,052) | 0 (167,232) / 1,210 (1,066) / 901,094 (901,238) | `473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5` |
| `kprincek.uftb` | King+Prince vs King | 14,324,280 | 867,040 (80,344) / 0 (38,536) / 0 | 0 (41,808) / 414,344 (492,944) / 36,808 (16) | `7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0` |
| `kgiantk.uftb` | King+Giant vs King | 4,747,504 | 3,300 (82,476) / 0 / 273,324 (133,860) | 0 (30,868) / 1,460 / 326,772 (133,860) | `eb52f2c08cf88e1e3682d0c72dfde191d9009e79779ad7eee23ca82fdcade591` |
| `kcopycatk.uftb` | King+Copycat vs King | 10,685,864 | 372,224 (108,184) / 0 / 12,552 | 0 (40,776) / 374,136 / 78,048 | `98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb` |
| `kdragonk.uftb` | King+Dragon vs King | 11,904,156 | 364,756 (128,204) / 0 / 0 | 0 (41,808) / 414,164 / 36,988 | `28d3cbeba82d02611a48bf2d0a6a527d11ff4cd3049f04bf2b4b929a05ed86c6` |
| `kjesterjesterk.uftb` | King+2 Jesters vs King | 231,295,032 | 7,259,568 (2,229,912) / 0 / 0 | 804,804 / 8,682,564 / 2,112 | `791453682cab1999efeeea44cc79b7d84be3b2658975a0e004cf589ca891e150` |
| `kjesterkjester.uftb` | King+Jester vs King+Jester | 489,320,832 | 6,639,760 / 493,966 / 11,845,234 | 6,639,760 / 493,966 / 11,845,234 | `243f0eaaf838774684082dadfeb69496b200e0061514239a83ccb771b879deee` |
| `kjesterknightk.uftb` | King+Jester+Knight vs King | 441,170,696 | 14,798,084 (4,180,876) / 0 / 0 | 1,609,608 / 16,055,072 / 1,314,280 | `4d9e7cd0ad349ef4ddc10f214ba3d5e63000d551b307a4e0e5266ee3950de9e6` |
| `kjesterkknight.uftb` | King+Jester vs King+Knight | 432,640,208 | 2,898,404 (3,093,244) / 4 / 12,987,308 | 2,808,980 / 121,472 / 16,048,508 | `cf6d15681c176ff90cd7c862187b5d1ab4db217c6b00468f032f57c817155d69` |
| `kjesterqueenk.uftb` | King+Jester+Queen vs King | 751,025,764 | 10,877,572 (8,101,388) / 0 / 0 | 1,609,608 / 17,307,456 / 61,896 | `b1e938905b645a70b2a2de433eb0ea12dc1374ad0bdef79738f115b65adfcb74` |
| `kjesterkqueen.uftb` | King+Jester vs King+Queen | 712,427,800 | 2,489,288 (3,093,244) / 7,673,186 / 5,723,242 | 16,716,862 / 2,682 / 2,259,416 | `1345e2b73a1978a6569821a379c236415997ff3b9cdb108563c982f458d88600` |
| `kjesterrookk.uftb` | King+Jester+Rook vs King | 595,252,940 | 12,797,700 (6,181,260) / 0 / 0 | 1,609,608 / 17,360,968 / 8,384 | `9e485cf5516add2e833527fbce24374380f444322b0db1e5a41e687bda500c82` |
| `kjesterkrook.uftb` | King+Jester vs King+Rook | 571,417,768 | 2,491,348 (3,093,244) / 1,291,354 / 12,103,014 | 10,758,014 / 4,634 / 8,216,312 | `ba1a6ecd7e71b6e3fbe1bff9a1576c0d4a9d0cc2f241aefd48ea519959ef984d` |
| `kjesterbishopk.uftb` | King+Jester+Bishop vs King | 504,293,320 | 13,965,588 (5,013,372) / 0 / 0 | 1,609,608 / 16,113,394 / 1,255,958 | `5816410c814d49a27dac924a0f5beb1b792b61b742a2fc83f0411bf10d15eae0` |
| `kjesterkbishop.uftb` | King+Jester vs King+Bishop | 489,530,528 | 2,506,936 (3,093,244) / 0 / 13,378,780 | 3,693,788 / 10,706 / 15,274,466 | `698f617b02ec063f481f57dac1b3677e7d9db618ca006dac59a3105c872dfd43` |
| `kjesterbombk.uftb` | King+Jester+Bomb vs King | 512,779,944 | 13,901,148 (5,077,812) / 0 / 0 | 1,609,608 / 17,331,368 / 37,984 | `5fdb70947be34ad506f893291c4657a4b0f850e39672caae6e564a1df71486fb` |
| `kjesterkbomb.uftb` | King+Jester vs King+Bomb | 494,687,920 | 95,410 (3,152,980) / 9,654,500 (62,344) / 6,013,726 | 15,947,500 / 29,024 / 3,002,436 | `b01b14d0a5d920e9c519c1c9a310882c7aec263c8a7aa71c2abbefb3202dc8cb` |
| `kjesterninjak.uftb` | King+Jester+Ninja vs King | 629,742,840 | 12,548,400 (6,430,560) / 0 / 0 | 1,609,608 / 17,313,736 / 55,616 | `33431727e988e9c2f48dc0f22e3418072fc8c6f741290b037fd1a5e28b0bbf97` |
| `kjesterkninja.uftb` | King+Jester vs King+Ninja | 602,269,760 | 2,499,692 (3,093,244) / 6,238,816 / 7,147,208 | 15,125,298 / 5,680 / 3,847,982 | `7255da99637afb9a015c284056b4be3431d602c6c0febddb7fd439157170a55b` |
| `kjesterturtlek.uftb` | King+Jester+Turtle vs King | 409,044,272 | 15,158,912 (3,820,048) / 0 / 0 | 1,609,608 / 16,006,830 / 1,362,522 | `496ab5eb6012ff5500b54b6589d153710839563621cbb4340e6f69534a0d3067` |
| `kjesterkturtle.uftb` | King+Jester vs King+Turtle | 402,759,128 | 8,899,984 (3,093,244) / 0 / 6,985,732 | 2,397,164 / 5,356,036 / 11,225,760 | `7216f1e140c32fc3ed69addce02b651f64ec3fef5173f7b0c11259fcc38ccebd` |
| `kjestermagek.uftb` | King+Jester+Mage vs King | 386,478,416 | 15,885,716 (3,093,244) / 0 / 0 | 1,609,608 / 15,952,244 / 1,417,108 | `9d48c76c568315e9ff533fe3d518f1c7fe528c421891b65b1e171657f2ed7c09` |
| `kjesterkmage.uftb` | King+Jester vs King+Mage | 364,406,212 | 15,885,716 (3,093,244) / 0 / 0 | 1,609,608 / 15,953,304 / 1,416,048 | `705b00311fc61e20615b79e13472b96cf7705f8e80227303877ec0445326d274` |
| `kjesterparasitek.uftb` | King+Jester+Parasite vs King | 462,590,064 | 14,519,136 (4,459,824) / 0 / 0 | 1,609,608 / 17,365,128 / 4,224 | `50d5e2a3a35f7746b27395ebeb922dd9583b10800b59a6870c18750416ac5630` |
| `kjesterkparasite.uftb` | King+Jester vs King+Parasite | 450,905,824 | 256,428 (3,093,244) / 15,592,716 / 36,572 | 18,877,766 / 87,668 / 13,526 | `45f0f26f182a2e231455df61931cf49d6a54d9b36f9be0e1fd9fd87117a7fd18` |
| `kjestergiantk.uftb` | King+Jester+Giant vs King | 259,948,596 | 9,347,536 (3,939,164) / 0 / 0 (5,692,260) | 1,142,116 / 11,300,932 / 843,652 (5,692,260) | `9cee141bd1bc8cd2e0427a95d6b96683e24e5096d2bafce7390750918a5eb1f8` |
| `kjesterkgiant.uftb` | King+Jester vs King+Giant | 265,285,348 | 10,986,856 (2,193,308) / 21,472 / 85,064 (5,692,260) | 3,091,428 / 7,778,226 / 2,417,046 (5,692,260) | `67a539cfcbeeb53837a6b7c07c497aed513aa9d12260a7588213eee783a204e6` |
| `kjesterfishermank.uftb` | King+Jester+Fisherman vs King | 796,241,336 | 15,885,716 (3,093,244) / 0 / 0 | 1,609,608 / 15,952,244 / 1,417,108 | `3f335332d457d184fc3a20b5ddddf2806911bb5e5bfd607872eb757a2a90a968` |
| `kjesterkfisherman.uftb` | King+Jester vs King+Fisherman | 723,303,740 | 2,499,754 (3,093,244) / 0 / 13,385,962 | 1,609,608 / 11,842 / 17,357,510 | `ce378df3a2be9b6fe0dcb82098a88abc7067ed1cde02821ca744af46dc461df0` |
| `kjesterdragonk.uftb` | King+Jester+Dragon vs King | 596,943,520 | 12,877,956 (6,101,004) / 0 / 0 | 1,609,608 / 17,350,088 / 19,264 | `3553fad7d75c3c428bbf0d9333801d57cb3dae0a99bc505ef1bf45de1f7cc017` |
| `kjesterkdragon.uftb` | King+Jester vs King+Dragon | 573,650,240 | 2,493,996 (3,093,244) / 7,153,948 / 6,237,772 | 16,036,060 / 5,426 / 2,937,474 | `55477de8dd1263ab734582dea3409af5409fa704bf35088d4ab4300beee7a5fb` |
| `kknightknightk.uftb` | King+2 Knights vs King | 196,460,680 | 360 (1,964,462) / 0 / 7,524,658 | 0 (804,804) / 68 / 8,684,608 | `5c95ba0ed74d95e4d2c2fa4d1a98c1dddb121a9d11406853c75d4d5e1ba15138` |
| `kknightqueenk.uftb` | King+Knight+Queen vs King | 674,814,946 | 11,144,698 (7,834,262) / 0 / 0 | 0 (1,609,608) / 15,991,680 / 1,377,672 | `5a2143cee6221564e0acaf53e5d0f5fd764f2997445fc8bbc490deb79c5693ba` |
| `kknightkqueen.uftb` | King+Knight vs King+Queen | 616,478,182 | 0 (2,808,960) / 13,559,860 / 2,610,140 | 11,888,556 (7,035,616) / 0 / 54,788 | `c708a791064b0b9f45bd506b3cfc1657912890dd1fb080533d311db6c3418750` |
| `kknightrookk.uftb` | King+Knight+Rook vs King | 533,160,532 | 13,069,652 (5,909,308) / 0 / 0 | 0 (1,609,608) / 16,041,948 / 1,327,404 | `04513bc582ac66c262ce0a50aadc7dc4a103cf281b5ae66aa06a73fd4a78d457` |
| `kknightkrook.uftb` | King+Knight vs King+Rook | 497,289,662 | 16 (2,808,960) / 1,871,092 / 14,298,892 | 6,652,526 (4,951,436) / 4 / 7,374,994 | `4f1655eb31c7a335de951bcc217a2e01b777872b2fc867fead6d42ed3f255da8` |
| `kknightbishopk.uftb` | King+Knight+Bishop vs King | 450,616,302 | 14,199,500 (4,733,914) / 0 / 45,546 | 0 (1,609,608) / 14,751,040 / 2,618,312 | `e78a30ba8605cccbbc3c9198dd717843ef80115905b7ea05b03f130756d54f48` |
| `kknightbombk.uftb` | King+Knight+Bomb vs King | 457,027,800 | 14,179,200 (4,799,760) / 0 / 0 | 0 (1,609,608) / 17,269,780 (67,760) / 31,812 | `6c1e33d2b92fe411485a2370fb18621c86d5e26de45a3deb053f0a4547312d72` |
| `kknightkbomb.uftb` | King+Knight vs King+Bomb | 431,449,080 | 264 (2,903,040) / 14,748,000 (62,768) / 1,264,888 | 15,062,940 (3,846,020) / 52 / 69,944 (4) | `64f63059b0d87f4b28bf8d30c0a1c3354ed42441fd6b84c6a1be26263aa9afde` |
| `kknightninjak.uftb` | King+Knight+Ninja vs King | 564,591,464 | 12,797,612 (6,181,348) / 0 / 0 | 0 (1,609,608) / 16,000,796 / 1,368,556 | `ecb39a6f313de06083df9552b6b46e291fdfda4f04c4536744096eea5615a3fa` |
| `kknightkninja.uftb` | King+Knight vs King+Ninja | 524,358,488 | 0 (2,808,960) / 13,522,364 / 2,647,636 | 13,657,124 (5,259,100) / 0 / 62,736 | `c72d856b445ae577e4394e9b0fe4ec3589a3c0fd55bd991d037a709bad688e78` |
| `kknightturtlek.uftb` | King+Knight+Turtle vs King | 363,952,568 | 14,093,700 (3,538,608) / 0 / 1,346,652 | 0 (1,609,608) / 12,261,896 / 5,107,456 | `e51001e72d79078c3da96de0dab87d8752d5f7496621b4b3812f82624107c5df` |
| `kknightparasitek.uftb` | King+Knight+Parasite vs King | 412,737,272 | 14,798,084 (4,180,876) / 0 / 0 | 0 (1,609,608) / 17,364,220 / 5,132 | `80703fb82f8caf5589e4e3d07bec5f6b4ce78dec12ce44d7a7f7e07d5e127246` |
| `kknightkparasite.uftb` | King+Knight vs King+Parasite | 396,499,600 | 20 (2,808,960) / 14,791,096 / 1,378,884 | 15,817,696 (3,093,244) / 4 / 68,016 | `c9c65bc38098b46e30464eabc4816c9d4e68c498b0876c7dca2df59103257c04` |
| `kknightgiantk.uftb` | King+Knight+Giant vs King | 228,702,604 | 9,415,136 (3,665,112) / 0 / 206,452 (5,692,260) | 0 (1,142,116) / 9,132,192 / 3,012,392 (5,692,260) | `2aecb1ceed4793a173c917fda61cb26bd622bf4dfec8e66d61c0e837f5cba541` |
| `kknightkgiant.uftb` | King+Knight vs King+Giant | 215,826,804 | 0 (1,968,832) / 19,388 / 11,298,480 (5,692,260) | 37,892 (3,051,612) / 0 / 10,197,196 (5,692,260) | `c3c8ceb56c11bbd52c3316e8dbb786e4fbe573382b6190b0e2596bf49ffda8fc` |
| `kknightdragonk.uftb` | King+Knight+Dragon vs King | 534,668,038 | 13,125,078 (5,853,878) / 0 / 4 | 0 (1,609,608) / 16,039,998 / 1,329,354 | `5a186985b6ca6e024562e2a0cff6dbe34003e30903761bae751400135abeb01c` |
| `kknightkdragon.uftb` | King+Knight vs King+Dragon | 498,929,176 | 0 (2,808,960) / 13,805,686 / 2,364,314 | 14,041,436 (4,893,140) / 0 / 44,384 | `22a14009f6d4f456eddcb659d3a5c1d99170ff4c706a517b786357abaecddabb` |
| `kqueenqueenk.uftb` | King+2 Queens vs King | 480,508,116 | 3,991,466 (5,498,014) / 0 / 0 | 0 (804,804) / 8,560,812 / 123,864 | `953ef23458f3681680b5d58736d854930218e59dea4961d8ddec5632108f1b21` |
| `kqueenkqueen.uftb` | King+Queen vs King+Queen | 701,876,100 | 4,619,002 (7,035,616) / 47,930 / 7,276,412 | 4,619,002 (7,035,616) / 47,930 / 7,276,412 | `41421585773f70182648370f85b441f6bb7ad39a43a1d9926b249a3c4f435fe1` |
| `kqueenrookk.uftb` | King+Queen+Rook vs King | 816,632,332 | 9,474,332 (9,504,628) / 0 / 0 | 0 (1,609,608) / 17,251,240 / 118,112 | `8674db75197f632ee8a9a0740b3569a49082929879fbd79ea70fe30efd10eaea` |
| `kqueenkrook.uftb` | King+Queen vs King+Rook | 656,954,204 | 11,847,368 (7,035,616) / 24,998 / 70,978 | 3,696,724 (4,951,436) / 9,667,734 / 663,066 | `e3616069bee07342fda4eed31660b8504ca8c0ac149038c8bbc35e10beb89a90` |
| `kqueenbishopk.uftb` | King+Queen+Bishop vs King | 732,953,056 | 10,451,944 (8,527,016) / 0 / 0 | 0 (1,609,608) / 16,034,380 / 1,334,972 | `8ce01962e4b7dde87d17de918bfe89ce50337bd353de2cd19ec8c2b99739774e` |
| `kqueenkbishop.uftb` | King+Queen vs King+Bishop | 633,491,052 | 11,915,826 (7,035,616) / 0 / 27,518 | 0 (3,693,788) / 12,200,676 / 3,084,496 | `3cb84ad6176d504554d46a67b62a03853d8590e6940ed9d66a6e983e0e2594f6` |
| `kqueenbombk.uftb` | King+Queen+Bomb vs King | 738,060,636 | 10,388,902 (8,590,058) / 0 / 0 | 0 (1,609,608) / 17,201,128 (67,760) / 100,464 | `a75bcbfedb3e5fc4f35d711e81b50db0b23d93ef232bdf1ee5b15c44787cf5d9` |
| `kqueenkbomb.uftb` | King+Queen vs King+Bomb | 629,403,948 | 1,421,172 (7,353,252) / 1,282,090 (46,244) / 8,873,510 (2,692) | 3,611,678 (3,842,796) / 670,572 (528) / 10,850,686 (2,700) | `7317a875224cf62e25de4b51c3bcabb3720fce8c0ebdab45e1db2cd2f4db9330` |
| `kqueenninjak.uftb` | King+Queen+Ninja vs King | 849,027,988 | 9,342,106 (9,636,854) / 0 / 0 | 0 (1,609,608) / 17,178,972 / 190,380 | `d9b0d5151b48d5137566ed0a0d7364af0122b1b68adfcc6d946afc9a31881f74` |
| `kqueenkninja.uftb` | King+Queen vs King+Ninja | 672,276,532 | 7,529,430 (7,035,616) / 9,456 / 4,404,458 | 3,622,556 (5,259,100) / 1,365,734 / 8,731,570 | `d406e996eabed529e140e6e31c93a5841561e412ce0350f8e36d169f0ddc8e4c` |
| `kqueenturtlek.uftb` | King+Queen+Turtle vs King | 644,511,080 | 11,369,834 (7,609,126) / 0 / 0 | 0 (1,609,608) / 15,954,050 / 1,415,302 | `c2a325eccaf941235c632d386beac95de4cdcc22b2005511b80e538f3353f39a` |
| `kqueenkturtle.uftb` | King+Queen vs King+Turtle | 606,531,770 | 11,943,312 (7,035,616) / 0 / 32 | 0 (2,397,164) / 14,534,402 / 2,047,394 | `353627128552df82c4eece2999e8e6805a11b29a418078c7f4f255f5e0ec55fe` |
| `kqueenmagek.uftb` | King+Queen+Mage vs King | 623,307,860 | 11,943,344 (7,035,616) / 0 / 0 | 0 (1,609,608) / 15,912,942 / 1,456,410 | `de3e8f1a0b0d1d5f216c8be8d4ab9efccf8a7a09a4c803175a4e6651c451a22e` |
| `kqueenkmage.uftb` | King+Queen vs King+Mage | 600,512,500 | 11,943,344 (7,035,616) / 0 / 0 | 0 (1,609,608) / 15,938,592 / 1,430,760 | `88a74961c77c7da83eded1c1d716037ffbe71a46746849651c3a308141d4b2f4` |
| `kqueenparasitek.uftb` | King+Queen+Parasite vs King | 694,110,316 | 10,877,572 (8,101,388) / 0 / 0 | 0 (1,609,608) / 17,307,456 / 61,896 | `6a29b00ab2a1edd510e94e67d5ca70d6da108b15bc9d9a3edd54b547d2b8327c` |
| `kqueenkparasite.uftb` | King+Queen vs King+Parasite | 622,639,914 | 2,353,096 (7,035,616) / 794,236 / 8,796,012 | 3,740,612 (3,093,244) / 1,561,098 / 10,584,006 | `f088d7d7937b6efed0cc69eec38fda1448bbb0351d135b9fb07027a9ad578d39` |
| `kqueengiantk.uftb` | King+Queen+Giant vs King | 411,446,456 | 7,071,014 (6,215,686) / 0 / 0 (5,692,260) | 0 (1,142,116) / 11,256,584 / 888,000 (5,692,260) | `b1e6c719cd0a03774e09c3bb700b63e54f1b6ad940780498c0d5ae3169302d85` |
| `kqueenkgiant.uftb` | King+Queen vs King+Giant | 358,244,540 | 8,452,246 (4,821,246) / 7,090 / 6,118 (5,692,260) | 20,818 (3,051,612) / 7,900,780 / 2,313,490 (5,692,260) | `e9ce180e7176f25be6857375977f5c4f56fc5b5b5805fa1c60a22e6a15510baa` |
| `kqueenfishermank.uftb` | King+Queen+Fisherman vs King | 998,335,056 | 11,943,344 (7,035,616) / 0 / 0 | 0 (1,609,608) / 15,912,942 / 1,456,410 | `d8999ba65bae1875637e5cc4e5b7ad857bb178533a7f41751cbdadce731f1c38` |
| `kqueenkfisherman.uftb` | King+Queen vs King+Fisherman | 870,038,166 | 11,943,344 (7,035,616) / 0 / 0 | 0 (1,609,608) / 15,953,468 / 1,415,884 | `c321eebe57da1b0b119a42001f06b98d1353528567fc9c6dffcd09b95c27e3b2` |
| `kqueendragonk.uftb` | King+Queen+Dragon vs King | 819,198,846 | 9,653,298 (9,325,662) / 0 / 0 | 0 (1,609,608) / 17,238,014 / 131,338 | `59dbc00aeff1770fdae48bb5ebeb6db85c4318e17d95f1fa531cc4fcf4c9f32e` |
| `kqueenkdragon.uftb` | King+Queen vs King+Dragon | 661,400,078 | 7,965,806 (7,035,616) / 108,042 / 3,869,496 | 4,290,082 (4,893,140) / 1,437,880 / 8,357,858 | `c4cf5caf8667b03528602cfa24f1b35e13bd1d038c5791fb028196965988d7cd` |
| `krookrookk.uftb` | King+2 Rooks vs King | 336,683,762 | 5,565,392 (3,924,088) / 0 / 0 | 0 (804,804) / 8,669,238 / 15,438 | `6ad45d2181b8bb5e7780e8bb87b654970778d108405c43d21c06954ed287a635` |
| `krookkrook.uftb` | King+Rook vs King+Rook | 574,799,708 | 3,748,496 (4,951,436) / 85,386 / 10,193,642 | 3,748,496 (4,951,436) / 85,386 / 10,193,642 | `74424fbc58c6b6e837780bed913ada2eb256cdcaf27fb53162c4b84e1e9d763e` |
| `krookbishopk.uftb` | King+Rook+Bishop vs King | 591,300,396 | 12,371,072 (6,607,888) / 0 / 0 | 0 (1,609,608) / 16,100,204 / 1,269,148 | `0c5930e4043e0581648739b7d5fc499ce18daec8d18e19f04836f27ca49cc2ad` |
| `krookkbishop.uftb` | King+Rook vs King+Bishop | 530,190,084 | 4,582,954 (4,951,436) / 0 / 9,444,570 | 0 (3,693,788) / 423,770 / 14,861,402 | `ce2f769cedcf748f0cf45fb78eb7f642c5df51e01f2aad7263a65238ab82bc56` |
| `krookbombk.uftb` | King+Rook+Bomb vs King | 596,543,578 | 12,221,516 (6,757,444) / 0 / 0 | 0 (1,609,608) / 17,242,972 (67,760) / 58,620 | `6af34f7ec80a32ea17a32a38a3ce217e620a3883a5d637e7077706c1b5260443` |
| `krookkbomb.uftb` | King+Rook vs King+Bomb | 529,230,264 | 91,214 (5,176,766) / 2,816,466 (54,846) / 10,838,228 (1,440) | 6,144,700 (3,844,836) / 15,106 (8) / 8,973,130 (1,180) | `8c427bef832972b822da38aedd9c2acbb86fe8f4e1a615c84644cf0e729f5121` |
| `krookninjak.uftb` | King+Rook+Ninja vs King | 705,763,068 | 11,008,382 (7,970,578) / 0 / 0 | 0 (1,609,608) / 17,271,872 / 97,480 | `a19e735be310cbf8caa6e8397d8202d134c16a51f9f5de486c3f4f56583584ea` |
| `krookkninja.uftb` | King+Rook vs King+Ninja | 597,070,432 | 3,833,886 (4,951,436) / 275,140 / 9,918,498 | 4,742,502 (5,259,100) / 131,520 / 8,845,838 | `f3b3c7caa225f800a73cdb302dec38601f02bd805700a1cbb001b41844b23492` |
| `krookturtlek.uftb` | King+Rook+Turtle vs King | 503,391,432 | 13,353,634 (5,625,326) / 0 / 0 | 0 (1,609,608) / 16,000,700 / 1,368,652 | `e2752a01a8c10a08344f8aa51373a2913a5c0175b48a476baccb907e7fc77526` |
| `krookkturtle.uftb` | King+Rook vs King+Turtle | 480,007,102 | 14,003,804 (4,951,436) / 4 / 23,716 | 4 (2,397,164) / 14,534,220 / 2,047,572 | `9250b5addb8e8b775422b8eb5d562337847f03ea08fe21cc660e2b7849045819` |
| `krookmagek.uftb` | King+Rook+Mage vs King | 482,774,292 | 14,027,524 (4,951,436) / 0 / 0 | 0 (1,609,608) / 15,950,608 / 1,418,744 | `b1704edd22b133cf0e3b036a69990353ce923d10f524aa3802d2ce54d03326ca` |
| `krookkmage.uftb` | King+Rook vs King+Mage | 462,063,112 | 14,027,524 (4,951,436) / 0 / 0 | 0 (1,609,608) / 15,952,876 / 1,416,476 | `f7f3e7b23902188d6c0e47673f8c59b483656de94486fa52a0affcb6ced4abd7` |
| `krookparasitek.uftb` | King+Rook+Parasite vs King | 552,591,676 | 12,797,700 (6,181,260) / 0 / 0 | 0 (1,609,608) / 17,360,968 / 8,384 | `91ca9a8f1a35291a384b84a2813e29433b331cb354039cd992d860cad7ac3c19` |
| `krookkparasite.uftb` | King+Rook vs King+Parasite | 508,640,722 | 100,882 (4,951,436) / 13,479,612 / 447,030 | 15,715,312 (3,093,244) / 17,690 / 152,714 | `05d30182c25ad00379ab4e5c1d51223cb108ee133c30d00efc89faa012e9aed2` |
| `krookgiantk.uftb` | King+Rook+Giant vs King | 321,003,062 | 8,246,894 (5,039,806) / 0 / 0 (5,692,260) | 0 (1,142,116) / 11,296,414 / 848,170 (5,692,260) | `36b212cd1ac7cc12ad2773453e30d9baef52b691488e5ffe777698708b8d0bc6` |
| `krookkgiant.uftb` | King+Rook vs King+Giant | 287,592,138 | 9,768,484 (3,443,644) / 13,856 / 60,716 (5,692,260) | 30,722 (3,051,612) / 7,888,652 / 2,315,714 (5,692,260) | `82ef14d4804794e0aba2a3d0faec6441e261f5200546f726f28e50bc1bbc72d6` |
| `krookfishermank.uftb` | King+Rook+Fisherman vs King | 857,801,488 | 14,027,524 (4,951,436) / 0 / 0 | 0 (1,609,608) / 15,950,608 / 1,418,744 | `093944252b6b71ad34c31a006493a6bb6ab46fef02238e6f1d4714dacf0e6707` |
| `krookkfisherman.uftb` | King+Rook vs King+Fisherman | 778,458,154 | 4,422,744 (4,951,436) / 0 / 9,604,780 | 0 (1,609,608) / 461,572 / 16,907,780 | `bd7c952e4779e7b9fd1e035dc1e9eabcd2202c51f0e96b0dfbe38bdd20b713a9` |
| `krookdragonk.uftb` | King+Rook+Dragon vs King | 676,425,340 | 11,413,200 (7,565,760) / 0 / 0 | 0 (1,609,608) / 17,325,054 / 44,298 | `7705efb335912c0ed7c8f7dd7c1ac63ea6cce8f353ce5f8f4799d8a09aa4f70c` |
| `krookkdragon.uftb` | King+Rook vs King+Dragon | 579,444,158 | 4,376,160 (4,951,436) / 437,638 / 9,213,726 | 5,782,264 (4,893,140) / 324,630 / 7,978,926 | `59d1e0f5d1a560efbd24fde36c9d5d6947e64ec50509b5500b56cff1b28644d8` |
| `kbishopbishopk.uftb` | King+2 Bishops vs King | 130,467,458 | 3,306,336 (1,498,130) / 0 / 3,376,686 (1,308,328) | 0 (407,502) / 3,708,210 / 4,976,466 (397,302) | `18ee411f10c1c62e365b9e30359a1403f5d34e827bcc5509f20d8c0e73ded490` |
| `kbishopbombk.uftb` | King+Bishop+Bomb vs King | 514,356,290 | 13,383,238 (5,595,722) / 0 / 0 | 0 (1,609,608) / 17,265,800 (67,760) / 35,792 | `7551d93b34162827fa0b35578b4c6a4d9acd181bddcaa31582ab1361756764b1` |
| `kbishopkbomb.uftb` | King+Bishop vs King+Bomb | 471,646,156 | 116 (3,850,520) / 13,139,856 (59,240) / 1,928,028 (1,200) | 14,983,656 (3,845,632) / 24 (16) / 149,256 (376) | `8f3edd16e534811b61a4dcd792f2bb7a26e7c6f43fb0397c9c3696de56d0ddbe` |
| `kbishopninjak.uftb` | King+Bishop+Ninja vs King | 622,467,416 | 12,053,584 (6,925,376) / 0 / 0 | 0 (1,609,608) / 16,048,930 / 1,320,422 | `25afeb53b8ab30386bf683f20c1769220d7e3b1c1356e543c60f1f260ad58985` |
| `kbishopkninja.uftb` | King+Bishop vs King+Ninja | 554,408,596 | 0 (3,693,788) / 12,195,462 / 3,089,710 | 13,679,960 (5,259,100) / 0 / 39,900 | `cd948abb01bd00c51ffdf0c4aebb46394b4f3052e6b86073c8b4e327fa813f22` |
| `kbishopturtlek.uftb` | King+Bishop+Turtle vs King | 421,326,784 | 14,555,124 (4,380,964) / 0 / 42,872 | 0 (1,609,608) / 14,694,114 / 2,675,238 | `356dc50f942ccd939ea3a1364b4481691b4abf9bb4338c65fef015b567bea827` |
| `kbishopmagek.uftb` | King+Bishop+Mage vs King | 400,784,944 | 0 (3,693,788) / 0 / 15,285,172 | 0 (1,609,608) / 0 / 17,369,352 | `4b32c84e4e56475370f5439dcd28b75a743e86531c8f27c4b652d47b722b767a` |
| `kbishopparasitek.uftb` | King+Bishop+Parasite vs King | 470,086,096 | 13,965,588 (5,013,372) / 0 / 0 | 0 (1,609,608) / 17,355,020 / 14,332 | `f47108fdea572a895b49d0e1ac7f94e342f0029a753a7aa3cd99217ae4d275fb` |
| `kbishopkparasite.uftb` | King+Bishop vs King+Parasite | 442,566,648 | 0 (3,693,788) / 13,243,670 / 2,041,502 | 15,738,520 (3,093,244) / 0 / 147,196 | `4974a88a0665613f8097467df8bf39c67a1048046963e7be6f0cbbd802e879b0` |
| `kbishopgiantk.uftb` | King+Bishop+Giant vs King | 263,060,894 | 5,668,664 (4,150,080) / 0 / 3,467,956 (5,692,260) | 0 (1,142,116) / 5,650,886 / 6,493,698 (5,692,260) | `f7bed858352d77238adb64d894d2e2ce4bc716703aa4354fa16f288708bd658c` |
| `kbishopkgiant.uftb` | King+Bishop vs King+Giant | 242,856,394 | 0 (2,519,718) / 15,910 / 10,751,072 (5,692,260) | 32,836 (3,051,612) / 0 / 10,202,252 (5,692,260) | `0693144e84471959edad34705a14dbef0685df3b60bc6d83960b904001133bb2` |
| `kbishopfishermank.uftb` | King+Bishop+Fisherman vs King | 775,812,140 | 0 (3,693,788) / 0 / 15,285,172 | 0 (1,609,608) / 0 / 17,369,352 | `3e2b9df328f5ae439a9d4559b83e5d7d80ae35c07004c9f3f6abccff7cc254f3` |
| `kbishopdragonk.uftb` | King+Bishop+Dragon vs King | 592,268,962 | 12,325,918 (6,653,042) / 0 / 0 | 0 (1,609,608) / 16,094,422 / 1,274,930 | `e5441c998ebc7f4ef1a7be2079a7919b96a2e02238a21ccd4b0e342925bfc6fd` |
| `kbishopkdragon.uftb` | King+Bishop vs King+Dragon | 531,451,376 | 8 (3,693,788) / 12,191,210 / 3,093,954 | 13,981,252 (4,893,140) / 4 / 104,564 | `8c0fba9b456b74d6784bd9b48932a1ccabf44237792ee57413bfbf55a95d2be0` |
| `kbombbombk.uftb` | King+2 Bombs vs King | 260,111,166 | 6,651,352 (2,838,050) / 0 (78) / 0 | 0 (804,804) / 8,582,770 (70,112) / 31,794 | `3007861257e32343194410f89e6c446a904086162f15481d46d7a5337404a4ab` |
| `kbombkbomb.uftb` | King+Bomb vs King+Bomb | 477,943,752 | 6,440,274 (3,841,414) / 4,746,170 (60,466) / 3,889,740 (896) | 6,440,274 (3,841,414) / 4,746,170 (60,466) / 3,889,740 (896) | `c32ed66d65ba864590c1539a383f2e74d703d9e0b7a221226bf91bde30816736` |
| `kbombninjak.uftb` | King+Bomb+Ninja vs King | 628,229,260 | 12,012,790 (6,966,170) / 0 / 0 | 0 (1,609,608) / 17,219,704 (67,760) / 81,888 | `6b3ae1d52b88034772c3ecc8d59303dfe2b66c23f199d70b1613060f8f0017f6` |
| `kbombkninja.uftb` | King+Bomb vs King+Ninja | 554,146,022 | 4,900,960 (3,842,796) / 489,142 (528) / 9,742,834 (2,700) | 1,190,298 (5,421,572) / 2,177,708 (53,280) / 10,136,102 | `3c4bbaa19cd78e42cc8d5f74c14ba98052ae381c0b705123357a11fd364ca9d5` |
| `kbombturtlek.uftb` | King+Bomb+Turtle vs King | 427,769,788 | 14,512,116 (4,466,844) / 0 / 0 | 0 (1,609,608) / 17,269,840 (67,760) / 31,752 | `ee4dca71935ad3215f55f9647542ca737f86a6b092ef36923ae7662032b827de` |
| `kbombkturtle.uftb` | King+Bomb vs King+Turtle | 411,556,236 | 15,132,776 (3,845,820) / 0 (4) / 160 (200) | 0 (2,437,986) / 15,822,016 (64,838) / 654,120 | `b3ed8e16f550dc7325b87bf02578d8b949f758f070a7c7e98cddfd741c107f91` |
| `kbombmagek.uftb` | King+Bomb+Mage vs King | 407,442,416 | 15,215,852 (3,763,108) / 0 / 0 | 0 (1,609,608) / 17,271,480 (67,760) / 30,112 | `17d8e361f33ffa0b7e2ea366d9a68711ea96a2115aab8fd4de09b55800f412af` |
| `kbombkmage.uftb` | King+Bomb vs King+Mage | 387,466,920 | 15,132,936 (3,846,024) / 0 / 0 | 0 (1,609,608) / 17,296,088 (67,760) / 5,504 | `a7469c100c45ef088a63fb7016ea648a2b93e8ed3793054d83961f364e92fbe3` |
| `kbombparasitek.uftb` | King+Bomb+Parasite vs King | 476,419,868 | 13,901,148 (5,077,812) / 0 / 0 | 0 (1,609,608) / 17,263,608 (67,760) / 37,984 | `2af30741c6d6e9892aac5bd68b397454f3e351f0e459d04c274e47a7f80d9a30` |
| `kbombkparasite.uftb` | King+Bomb vs King+Parasite | 447,255,924 | 6,976,158 (3,842,796) / 3,007,200 (3,224) / 5,149,578 (4) | 5,374,364 (3,093,268) / 5,237,688 (59,260) / 5,211,320 (3,060) | `bc300fdbcb2945fd8fe494f09337a39f28fe7f6c41debac8535a7b124fca3d5f` |
| `kbombgiantk.uftb` | King+Bomb+Giant vs King | 273,239,456 | 8,936,526 (4,350,174) / 0 / 0 (5,692,260) | 0 (1,142,116) / 12,067,988 (48,420) / 28,176 (5,692,260) | `50ad69316185c89f66f3f15d61ce3f6f969419eea73b5dbd331cc6d831cbae5e` |
| `kbombkgiant.uftb` | King+Bomb vs King+Giant | 249,537,360 | 9,796,534 (2,726,186) / 19,520 / 744,452 (5,692,268) | 30,670 (3,121,728) / 7,645,810 (43,224) / 2,444,248 (5,693,280) | `b504391775e091cd8ca49bca0763e9359262ad794a7f9b6cd79d80d66a6fb9f1` |
| `kbombfishermank.uftb` | King+Bomb+Fisherman vs King | 781,013,976 | 15,215,852 (3,763,108) / 0 / 0 | 0 (1,609,608) / 17,271,480 (67,760) / 30,112 | `c35a204b36baf78a0c823f857b370adeb4a22a2aa1b7eb159cface00da7e5b6b` |
| `kbombkfisherman.uftb` | King+Bomb vs King+Fisherman | 729,381,942 | 3,883,058 (3,845,724) / 0 / 11,249,878 (300) | 0 (1,609,608) / 1,468,590 (42,504) / 15,833,002 (25,256) | `afdd6e53d8f69c864f9c4dd29d44082319d434640b52157181cf078a31ddbf72` |
| `kbombdragonk.uftb` | King+Bomb+Dragon vs King | 598,544,858 | 12,346,586 (6,632,374) / 0 / 0 | 0 (1,609,608) / 17,256,790 (67,760) / 44,802 | `062163e51a367a6558d6cf520594ea40fcf3ba1f4d1572ea4a8eb62bcbd2f9ff` |
| `kbombkdragon.uftb` | King+Bomb vs King+Dragon | 531,583,888 | 4,442,892 (3,843,440) / 116,256 (264) / 10,573,788 (2,320) | 510,846 (5,087,512) / 1,891,844 (54,248) / 11,433,310 (1,200) | `6b1c65f252af9e836c6ccf51a368baaca815bffeeb2d1efca8034cf8c85a900e` |
| `kninjaninjak.uftb` | King+2 Ninjas vs King | 368,966,508 | 5,424,306 (4,065,174) / 0 / 0 | 0 (804,804) / 8,609,904 / 74,772 | `18fd0757d22df706de52658fc16ff2448be7ddb042e4c84b5561983c1031d6c8` |
| `kninjakninja.uftb` | King+Ninja vs King+Ninja | 616,537,904 | 3,731,190 (5,259,100) / 42,886 / 9,945,784 | 3,731,190 (5,259,100) / 42,886 / 9,945,784 | `f6decd9288f45723c055e89e2af71dee4a02865fbaea5b8256952131cfe74898` |
| `kninjaturtlek.uftb` | King+Ninja+Turtle vs King | 534,851,632 | 13,095,300 (5,883,660) / 0 / 0 | 0 (1,609,608) / 15,961,482 / 1,407,870 | `d74dad4d2f24688128956f8a6953f18bfadd5db0d3662844c3e8c99d0f888326` |
| `kninjakturtle.uftb` | King+Ninja vs King+Turtle | 508,867,664 | 13,719,744 (5,259,100) / 0 / 116 | 0 (2,397,164) / 14,534,186 / 2,047,610 | `5aa6d5de8a796b51006eccdaddb13ed8054a9339bc05c05371069365d7358a1a` |
| `kninjamagek.uftb` | King+Ninja+Mage vs King | 513,941,200 | 13,719,860 (5,259,100) / 0 / 0 | 0 (1,609,608) / 15,914,976 / 1,454,376 | `47d3661b0f889b224609320c39b595ceb0acd7bf96b4842f41c73118ee7c50a9` |
| `kninjakmage.uftb` | King+Ninja vs King+Mage | 492,922,356 | 13,719,860 (5,259,100) / 0 / 0 | 0 (1,609,608) / 15,942,740 / 1,426,612 | `8686c7bcc2093ef3812ced89ef620f6f01a876a72b8b928aae23fce957f510be` |
| `kninjaparasitek.uftb` | King+Ninja+Parasite vs King | 584,118,216 | 12,548,400 (6,430,560) / 0 / 0 | 0 (1,609,608) / 17,313,736 / 55,616 | `c37a2962c07c0e6317ff0db96a1e742c97a63259fe708a80f9423b013d857a60` |
| `kninjakparasite.uftb` | King+Ninja vs King+Parasite | 535,099,432 | 2,199,160 (5,259,100) / 1,828,856 / 9,691,844 | 5,070,022 (3,093,244) / 1,201,524 / 9,614,170 | `3b3eec23f0e5f96eb845c436bf35aabc5eda9b870d96411981f7e8023e5e6c6e` |
| `kninjagiantk.uftb` | King+Ninja+Giant vs King | 344,953,212 | 8,088,164 (5,198,536) / 0 / 0 (5,692,260) | 0 (1,142,116) / 11,260,640 / 883,944 (5,692,260) | `b43c74ea48ece9cbba5c344193b2db26044735da0f7a32f70b86f734206fb1df` |
| `kninjakgiant.uftb` | King+Ninja vs King+Giant | 306,835,628 | 9,568,874 (3,695,184) / 13,504 / 9,138 (5,692,260) | 29,382 (3,051,612) / 7,890,026 / 2,315,680 (5,692,260) | `3a41d7ff3cd4906c814bf8e6bd3af56ec30079cbdd4e43b8ed7c523edff4225b` |
| `kninjafishermank.uftb` | King+Ninja+Fisherman vs King | 888,968,396 | 13,719,860 (5,259,100) / 0 / 0 | 0 (1,609,608) / 15,914,976 / 1,454,376 | `2adcd8f6d511a359b43a9dbb89650b0e2492d338d6d98538208796a182df0763` |
| `kninjakfisherman.uftb` | King+Ninja vs King+Fisherman | 803,024,716 | 13,719,860 (5,259,100) / 0 / 0 | 0 (1,609,608) / 15,953,476 / 1,415,876 | `de5c29df362dc35024077e4dcd380fffe6836e469d863e3871a7a1b52549be09` |
| `kninjadragonk.uftb` | King+Ninja+Dragon vs King | 707,856,384 | 11,131,336 (7,847,624) / 0 / 0 | 0 (1,609,608) / 17,252,356 / 116,996 | `de0501b3bef6a6abf8fc81c478c31af9a6c3f71f28d313473de954dc62beb52c` |
| `kninjakdragon.uftb` | King+Ninja vs King+Dragon | 599,564,588 | 4,589,520 (5,259,100) / 186,156 / 8,944,184 | 4,806,436 (4,893,140) / 276,566 / 9,002,818 | `b95ef8b87226cdbcbd0181443086fd4bb55033760d1e3cbc4fff0512141bdf5e` |
| `kturtleturtlek.uftb` | King+2 Turtles vs King | 167,494,620 | 440 (1,578,876) / 0 / 7,910,164 | 0 (804,804) / 416 / 8,684,260 | `bbb146a2f252eaf0eb40d23e49d0ddda56de183fb202c515ff64a0fd172037b1` |
| `kturtleparasitek.uftb` | King+Turtle+Parasite vs King | 383,586,096 | 15,158,912 (3,820,048) / 0 / 0 | 0 (1,609,608) / 17,364,876 / 4,476 | `eecd62d724dc6c09c12d13d3170a88c951b884bb7c6fed5c522c6085a763a0ae` |
| `kturtlekparasite.uftb` | King+Turtle vs King+Parasite | 373,105,856 | 0 (2,397,164) / 7,602,610 / 8,979,186 | 9,995,776 (3,093,244) / 0 / 5,889,940 | `7e174a2eaa35113df49bbd5b8dd89e16eac9359a96e93b9032d7c1622de8d878` |
| `kturtlegiantk.uftb` | King+Turtle+Giant vs King | 210,345,328 | 8,823,608 (3,504,624) / 0 / 958,468 (5,692,260) | 0 (1,142,116) / 7,851,670 / 4,292,914 (5,692,260) | `f84a9bc0224441dbb0d78d0fac8fbad6a92ff7159cc9bab1772ccece4f03911a` |
| `kturtlekgiant.uftb` | King+Turtle vs King+Giant | 201,853,704 | 0 (1,705,440) / 24,002 / 11,557,258 (5,692,260) | 42,846 (3,051,612) / 4 / 10,192,238 (5,692,260) | `08adee4b8b5903dac5038a2d70abed1be2b75773f1ae0b64d5d432d63ef21cc6` |
| `kturtledragonk.uftb` | King+Turtle+Dragon vs King | 505,144,192 | 13,456,552 (5,522,408) / 0 / 0 | 0 (1,609,608) / 15,996,498 / 1,372,854 | `5a40d96d2a1f2747a0e246d52add88cd99496a7a6d3e796fc1e11c0bec5a4f36` |
| `kturtlekdragon.uftb` | King+Turtle vs King+Dragon | 481,941,236 | 0 (2,397,164) / 14,530,158 / 2,051,638 | 14,056,448 (4,893,140) / 0 / 29,372 | `15fd11a4865807c48c1d47b508b5073f78efdf12332b34ab8ff7fd062d567fca` |
| `kmageparasitek.uftb` | King+Mage+Parasite vs King | 363,306,160 | 15,885,716 (3,093,244) / 0 / 0 | 0 (1,609,608) / 17,368,120 / 1,232 | `d3d34227988697a1416e50aba326819bd14915ff18f9e5794846c477488ab076` |
| `kmagekparasite.uftb` | King+Mage vs King+Parasite | 344,453,172 | 0 (1,609,608) / 17,369,180 / 172 | 15,885,716 (3,093,244) / 0 / 0 | `7d8638ada4d6e2fb5314afc7b87bf25a4013f149b76525f44ba561a55cf147aa` |
| `kmagegiantk.uftb` | King+Mage+Giant vs King | 212,650,924 | 9,344,454 (3,939,164) / 0 / 3,082 (5,692,260) | 0 (1,142,116) / 9,720,526 / 2,424,058 (5,692,260) | `84c017bbc31770a08feee5bc2c35d57783594af5b11916178076314a963ffb22` |
| `kmagekgiant.uftb` | King+Mage vs King+Giant | 182,439,080 | 0 (1,142,116) / 11,618 / 12,132,966 (5,692,260) | 31,716 (3,051,612) / 0 / 10,203,372 (5,692,260) | `c456f597bd6c281aa69b447da488e97cc9b7f274bc8ecba4e243ad676f66b959` |
| `kmagedragonk.uftb` | King+Mage+Dragon vs King | 484,234,160 | 14,085,820 (4,893,140) / 0 / 0 | 0 (1,609,608) / 15,945,268 / 1,424,084 | `d3733f70585208428185796973f80c171dc0f6b83adcc9923f46832f6fb73150` |
| `kmagekdragon.uftb` | King+Mage vs King+Dragon | 463,581,276 | 0 (1,609,608) / 15,951,230 / 1,418,122 | 14,085,820 (4,893,140) / 0 / 0 | `37521b0c37cfcd7cfb313f0532649eb0e9d4007a15c6d2524d34295b0c3790ae` |
| `kparasiteparasitek.uftb` | King+2 Parasites vs King | 216,128,928 | 7,259,568 (2,229,912) / 0 / 0 | 0 (804,804) / 8,682,564 / 2,112 | `000efc03710b9ed9771056a3c2d6490803739a163475094580fe499b39527e70` |
| `kparasitekparasite.uftb` | King+Parasite vs King+Parasite | 412,490,816 | 5,802,628 (3,093,244) / 3,262,986 / 6,820,102 | 5,802,628 (3,093,244) / 3,262,986 / 6,820,102 | `3d8117f1323b75f1b183355ca1eff44c122b40b7647d65ca24135c13be30169e` |
| `kparasitegiantk.uftb` | King+Parasite+Giant vs King | 243,180,684 | 9,347,536 (3,939,164) / 0 / 0 (5,692,260) | 0 (1,142,116) / 12,141,428 / 3,156 (5,692,260) | `7b0536386bd6b8525b9054578edba1331f8daabadb6089a555e05001572b926a` |
| `kparasitekgiant.uftb` | King+Parasite vs King+Giant | 227,572,076 | 4,429,348 (2,193,308) / 22,222 / 6,641,822 (5,692,260) | 39,116 (3,051,612) / 1,811,896 / 8,384,076 (5,692,260) | `93b215613d5bd0322ec937aac8f875aa62f4642d913a6f5bf5c43abe0461d434` |
| `kparasitefishermank.uftb` | King+Parasite+Fisherman vs King | 738,333,356 | 15,885,716 (3,093,244) / 0 / 0 | 0 (1,609,608) / 17,368,120 / 1,232 | `6b18865fd3c4e670c68a7ad5845239dece0fabbcbb3fad83c456018b5e274a03` |
| `kfishermankparasite.uftb` | King+Fisherman vs King+Parasite | 703,350,700 | 0 (1,609,608) / 314,868 / 17,054,484 | 1,742,400 (3,093,244) / 0 / 14,143,316 | `6032d4e60931298ce7fc31341cde1318a4fdcd19604c29a1a260a577d0f4c389` |
| `kparasitedragonk.uftb` | King+Parasite+Dragon vs King | 554,255,912 | 12,877,956 (6,101,004) / 0 / 0 | 0 (1,609,608) / 17,350,088 / 19,264 | `2b91e50eb9ff938d3f0419b34bff35c5496b5c1ee077949f09a46bb4d8090ca2` |
| `kparasitekdragon.uftb` | King+Parasite vs King+Dragon | 510,498,792 | 4,898,546 (3,093,244) / 314,622 / 10,672,548 | 887,716 (4,893,140) / 1,657,392 / 11,540,712 | `5259021302823eed33bf757a63b2c32caf8f07a45ad619e0fa9ab52af20df75e` |
| `kgiantgiantk.uftb` | King+2 Giants vs King | 62,426,868 | 1,713,626 (1,555,208) / 0 / 1,196,498 (5,024,148) | 0 (389,094) / 1,722,850 / 2,353,388 (5,024,148) | `1c737dec95c4c28c2127edf5bf190aa000687d95d19360175b9aa8c561906b59` |
| `kgiantkgiant.uftb` | King+Giant vs King+Giant | 117,385,888 | 35,570 (2,060,572) / 15,798 / 6,818,724 (10,048,296) | 34,000 (2,060,572) / 15,770 / 6,820,322 (10,048,296) | `d5503ba4bfd875b28958d3ffc5f7a6c5b77ebe0cd9b952a37dd7923126df4302` |
| `kgiantfishermank.uftb` | King+Giant+Fisherman vs King | 441,838,754 | 10,079,764 (3,097,480) / 0 / 109,456 (5,692,260) | 0 (1,142,116) / 9,407,898 / 2,736,686 (5,692,260) | `d6322a6a99b4876cb25e05b88317a7b32cd762065c54c9415bfbd3ce01127ffe` |
| `kgiantkfisherman.uftb` | King+Giant vs King+Fisherman | 398,427,880 | 30,332 (3,051,612) / 0 / 10,204,756 (5,692,260) | 0 (1,313,848) / 12,816 / 11,960,036 (5,692,260) | `2e999ecfec7aa2dd0f604edc406ec77e79f39a5251f13a65065181486522a64d` |
| `kgiantdragonk.uftb` | King+Giant+Dragon vs King | 319,145,998 | 8,445,404 (4,840,992) / 0 / 304 (5,692,260) | 0 (1,142,116) / 11,293,652 / 850,932 (5,692,260) | `12c16784095c23ca9aefd21ebb6322477be1659d36001e459b534d9713af9fab` |
| `kgiantkdragon.uftb` | King+Giant vs King+Dragon | 286,479,206 | 31,726 (3,051,612) / 7,071,638 / 3,131,724 (5,692,260) | 9,668,900 (3,346,434) / 12,978 / 258,388 (5,692,260) | `bf638c7b6b3082eb9c9899a67c6e3a7cb25dfd80ac2a7ba251b184fc0edc4a80` |
| `kfishermandragonk.uftb` | King+Fisherman+Dragon vs King | 859,261,356 | 14,085,816 (4,893,140) / 0 / 4 | 0 (1,609,608) / 15,945,260 / 1,424,092 | `0c860c29d234c4d0cb13106abe1fb3fb0281697533d8b6e1eb5c90cb9ea19f70` |
| `kfishermankdragon.uftb` | King+Fisherman vs King+Dragon | 782,325,302 | 0 (1,609,608) / 15,953,310 / 1,416,042 | 14,085,820 (4,893,140) / 0 / 0 | `949f749c991ae2901b0cc9bad4acb8fababef28a5e3e4b62e483211a1d467c41` |
| `kdragondragonk.uftb` | King+2 Dragons vs King | 338,720,772 | 5,682,590 (3,806,890) / 0 / 0 | 0 (804,804) / 8,655,726 / 28,950 | `fcbd96c4751483c15c31577d3850fd3270ddaac00d7a96fe5495a60f49ede102` |
| `kdragonkdragon.uftb` | King+Dragon vs King+Dragon | 580,885,096 | 3,622,924 (4,893,140) / 110,414 / 10,352,482 | 3,622,924 (4,893,140) / 110,414 / 10,352,482 | `a8740a9fc8683cd75bcd319081c544e1067796ad37b401a2259f2c6799847b56` |
| `kbombghostk.uftb` | King+Bomb+Ghost vs King | 969,368,924 | 29,117,000 (8,840,920) / 0 / 0 | 0 (3,219,216) / 33,065,732 (1,612,016) / 59,232 (1,724) | `25fc8d85b05c1104b50118cc7486bd3cfb8bb853eeedbb748cc81d29b95fa86b` |
| `kbombprincek.uftb` | King+Bomb+Prince vs King | 678,856,408 | 31,306,800 (5,077,812) / 0 (1,545,980) / 27,328 | 0 (1,609,608) / 17,263,608 (19,045,852) / 37,984 (868) | `74b48061b11795d274bcbb421019558cedc9fb1decb422c967020c9a8f9f827e` |
| `kpawnbombk.uftb` | King+Pawn+Bomb vs King | 779,725,448 | 26,783,928 (11,173,990) / 0 / 2 | 0 (3,219,216) / 31,088,884 (3,586,976) / 56,952 (5,892) | `804da1e2b87eac100ac6101ea2fe10fc2e83adeb8d0e4f50143f3a89719f3684` |
| `kbombcheckerk.uftb` | King+Bomb+Checker vs King | 818,127,549 | 29,074,749 (9,725,859) / 0 (3,286,402) / 0 (33,828,830) | 0 (3,219,216) / 32,817,112 (10,068,704) / 57,398 (29,753,410) | `013330f8184618b0d536da4a4e9417c8fbc1b49fcaab2a1842266e8246b7ad52` |
| `kbombsniperk.uftb` | King+Bomb+Sniper vs King | 1,509,762,618 | 29,596,516 (46,319,320) / 0 / 2 (2) | 0 (6,438,432) / 34,536,086 (34,802,038) / 67,098 (72,186) | `be04af3e927cf0a34219b0bd2b3663e659da16c86be7775ee8afb05d55150b9e` |
| `kbishopghostk.uftb` | King+Bishop+Ghost vs King | 958,964,780 | 29,226,628 (8,731,292) / 0 / 0 | 0 (3,219,216) / 32,002,462 (1,482,452) / 1,252,606 (1,184) | `61bc4f9865d4c217868da670e27e16e9a493f31ca15ae6a68f91469b8c48dcf2` |
| `kbishopkghost.uftb` | King+Bishop vs King+Ghost | 931,510,904 | 0 (7,430,290) / 16,770 (229,350) / 29,208,176 (1,073,334) | 6,505,412 (4,702,852) / 0 / 26,749,656 | `0649b7859ba72c8534929a902f18b3ca1a74cd7d7b3713ac109633e83a85ac15` |
| `kbishopkprince.uftb` | King+Bishop vs King+Prince | 648,044,796 | 0 (3,693,788) / 12,149,366 (18,668,874) / 3,135,806 (310,086) | 31,533,972 (3,093,244) / 0 (3,082,494) / 248,210 | `e3e2ae7d3c017eeaf0b0365b7bc1114170d0b076ea55dbc83ff66ebbbf318fb9` |
| `kbishopprincek.uftb` | King+Bishop+Prince vs King | 673,430,482 | 31,456,226 (5,013,372) / 0 (1,483,636) / 4,686 | 0 (1,609,608) / 16,113,394 (18,972,918) / 1,255,958 (6,042) | `bdfb6056b073a1c261379d0e2d56ffdc30f3422cf6ed5de00a912cfe3a117116` |
| `kbombkghost.uftb` | King+Bomb vs King+Ghost | 941,791,120 | 22,593,234 (8,298,186) / 49,906 (130,734) / 6,398,378 (487,482) | 1,782,612 (4,708,268) / 18,679,062 (124,688) / 12,663,290 | `29305aa38fce6a02b0ca1dca724388c02d6e8a4ec5002edc123248c59850bfd0` |
| `kbombkprince.uftb` | King+Bomb vs King+Prince | 648,908,976 | 5,503,540 (7,726,596) / 4,450,992 (10,145,276) / 5,178,404 (4,953,112) | 17,973,414 (3,162,508) / 5,324,652 (3,453,840) / 8,043,154 (352) | `cd220ce57a5db672809cf550f82f9d06f2195e60a001f6fe2e6c5186fabf8b94` |
| `kghostdragonk.uftb` | King+Ghost+Dragon vs King | 1,125,863,212 | 26,939,644 (11,018,276) / 0 / 0 | 0 (3,219,216) / 33,239,122 (1,480,412) / 15,946 (3,224) | `863de816680ef282923aca7cf9a5bf0b60cd76fa01dc7c2ffdf6a64e51a3b236` |
| `kghostfishermank.uftb` | King+Ghost+Fisherman vs King | 1,501,353,596 | 33,255,068 (4,702,852) / 0 / 0 | 0 (3,219,216) / 31,839,192 (1,483,020) / 1,415,876 (616) | `94db43bfad0d5e40f35e5cae91ddff67789f90741f28f5ebef357fc10fd77dea` |
| `kghostghostk.uftb` | King+2 Ghosts vs King | 898,829,504 | 31,829,960 (6,127,960) / 0 / 0 | 0 (3,219,216) / 31,829,960 (2,907,496) / 0 (1,248) | `204f4de6d0f9ff6da111d3d0c0a08c3562493cdec2130cc946b4eae7183012ee` |
| `kghostgiantk.uftb` | King+Ghost+Giant vs King | 497,645,576 | 19,660,036 (6,913,364) / 0 / 0 (11,384,520) | 0 (2,284,232) / 22,395,792 (1,049,920) / 842,184 (11,385,792) | `5261c5a3d62f8711e2d6e007b89788701a7cdd2ab2afda0edea5607514e4c793` |
| `kghostkdragon.uftb` | King+Ghost vs King+Dragon | 1,083,015,640 | 6,472,220 (4,702,852) / 1,136,062 / 25,646,786 | 7,655,934 (9,832,194) / 5,792 (213,998) / 19,266,524 (983,478) | `f9e825a80062da30fb4ffcb40ad7c9e4cf7348e03f2269e83812c34925080225` |
| `kghostkfisherman.uftb` | King+Ghost vs King+Fisherman | 1,501,353,596 | 6,480,254 (4,702,852) / 0 / 26,774,814 | 0 (3,219,216) / 12,160 (154,200) / 33,242,908 (1,329,436) | `ed56e8ccba231dbe962870891330ebae25495c3693e010b7aa874d511183c389` |
| `kghostkgiant.uftb` | King+Ghost vs King+Giant | 472,896,720 | 23,023,276 (3,335,424) / 44,444 / 170,256 (11,384,520) | 79,414 (6,103,488) / 15,513,330 (810,214) / 3,992,004 (11,459,470) | `452b47d680ae8c40b4136d33c3398145909c68119a9fe1c53cde0e7c4c5e7a63` |
| `kghostkmage.uftb` | King+Ghost vs King+Mage | 709,666,128 | 33,255,068 (4,702,852) / 0 / 0 | 0 (3,219,216) / 31,839,192 (1,483,584) / 1,415,876 (52) | `d23e1c2253e1ff26822069f5852989c73c1ab308b3720ce4a5473fa29c881519` |
| `kghostkparasite.uftb` | King+Ghost vs King+Parasite | 861,905,568 | 1,997,810 (4,702,852) / 31,182,462 / 74,796 | 30,220,468 (7,390,752) / 167,760 (147,194) / 26,748 (4,998) | `18c6dc8986ac43d9f38315bbc9e0c2ab6b0bed32c6c638991be01478cc2aceff` |
| `kghostmagek.uftb` | King+Ghost+Mage vs King | 744,404,832 | 33,255,068 (4,702,852) / 0 / 0 | 0 (3,219,216) / 31,839,192 (1,483,020) / 1,415,876 (616) | `061a250c66c18e769baec7410be2257db98e669cf6b3d6b39cd5153b27792596` |
| `kghostparasitek.uftb` | King+Ghost+Parasite vs King | 881,036,992 | 30,404,852 (7,553,068) / 0 / 0 | 0 (3,219,216) / 33,252,652 (1,482,156) / 2,416 (1,480) | `1bb1c977b7af34b2295dd6c693d8a79006e69ab2f4291a828b930515742222ca` |
<!-- GENERATED_TABLE_END -->

Each W / L / D cell is from the perspective of the side to move named by its
column. Parentheses separate unreachable dense-index states and exclude them
from the preceding legal-result count. Every generated row has a SHA-bound
native audit using the same royal-safety simulation as move legality: at an
ordinary turn boundary, the previous mover cannot have left its real King
threatened unless that side still owns a live Jester. The audit is
outcome-independent and includes indirect royal kills through Bomb chains and
other character effects. Stateful audits union that condition by state identity
with recovered causal failures such as impossible promotion squares, hidden
Ghost adjacency, Sniper cooldown/turn parity, an active Penguin aura with no
frozen model, an impossible forced-action turn, or a turn that could not survive
the frozen lone enemy King. A Jester exemption is color-specific; the 41,808
bare-King-side wins in `kjesterk` remain legal because the previous mover owns
the Jester.

Bundled files use packed format version 4 (or v5 when a character has a second
linked/material model): a two-bit WDL plane, one-byte DTW
plane, and a sparse exact exception stream for distances of 255 or more. This
is 37.5% smaller than the former 16-bit records before entropy coding. Every file
was validated after retrograde propagation by
regenerating all legal successors and checking the Bellman equations for every
state. Giant anchor tuples whose 2x2 footprint is off-board or overlaps a King
are explicit draw sentinels and can never be produced by the probe.

The state dimensions preserve Ghost visibility, Sniper cooldown, both Prince
action phases, Pawn moved state, and the Penguin's inactive/active exact
geometry-derived freeze aura. Penguins have no native cooldown. Pawn
promotions cross-probe the exact Queen table. A double-step
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

Regenerate them with `tools/generate_ultimate_tablebases.sh`; checkpoints are
piece-tagged and resumable. After generation,
`tools/audit_ultimate_tablebase_reachability.py` records the
native turn-boundary audit against each immutable table SHA-256 before
`tools/update_ultimate_tablebase_readme.py --full` rebuilds this summary.
Lone Bishop, Knight, Turtle, Mage, Devil, Sludge,
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

The 10 GiB budget additionally admits 29 stateful classes: K+Bomb+Ghost,
K+Bomb+Penguin, K+Bomb+Prince, K+Pawn+Bomb, K+Bomb+Checker, K+Bomb+Sniper,
K+Berserker+Bomb, K+Bishop+Ghost, K+Bishop versus K+Ghost, K+Bishop versus
K+Penguin, K+Bishop versus K+Prince, K+Bishop+Penguin, K+Bishop+Prince,
K+Bomb versus K+Ghost, K+Bomb versus K+Penguin, K+Bomb versus K+Prince,
K+Ghost+Dragon, K+Ghost+Fisherman, K+Ghost+Ghost, K+Ghost+Giant, K+Ghost
versus K+Dragon, K+Ghost versus K+Fisherman, K+Ghost versus K+Giant,
K+Ghost versus K+Mage, K+Ghost versus K+Parasite, K+Ghost+Mage,
K+Ghost+Parasite, K+Jester+Ghost, and K+Jester versus K+Ghost. Devil, Sludge,
Angel, and Copycat
combinations are excluded from K+K+2 because Minion spawning, persistent
Goop, Angel host/Halo state, and Copycat's linked clone make those classes
larger than the exact four-model closure used here; representing them as
ordinary K+K+2 tables would be incorrect.
