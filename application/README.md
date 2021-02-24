# Implementation de la soluton technique de la thèse de master pour le MSE


1. AG doit finir avec des données fiables
2. AG ne doit pas être capable de lier plusieurs mesures venant d'un même device entre elles.
3. D1 ne doit pas pouvoir identifier ou connaitre la position de D2

Pour éviter qu’un ensemble de mesures puis être relié à un appareil, il nous faut trouver un moyen d’avoir une unicité d’identification. Un hash d’un message comprenant l’identifiant unique, une information séquentielle ainsi qu’une valeur aléatoire nous permet de rendre unique ce numéro, qu’on appellera `MUID` pour Measure Unique IDentifier. Un appareil ne produira pas deux datas point identiquement identifié, et deux appareils ne se feront jamais concurrence quant à la production d’un MUID similaire. L’ajout d’une information évoluant temporellement nous permet de garantir la non-production de deux données similaires si le nombre aléatoire était tiré deux fois identiquement dans la vie de l’appareil, qui statistiquement est improbable, mais possible. La structure de mesure est donc la suivante: 
```C
struct measure {
    // identifiant de mesure unique, résultat d'une fonction à sens unique
    uint64_t MUID;          

    // timestamp de la mesure
    datetime_t timestamp;   

    // position de la mesure
    GPS_point_t position;   

    // contant les mesures de pollution
    datapoint_t data;       

    // fiabilité de la mesure
    float reliablilty;      
};
```

* D1 possède un certificat $C_{D1}$ qu'il partage avec l'AG, permettant la communication sécurisée entre les deux. D2 partage aussi un certificat $C_{D2}$ avec l'AG. 

* L'AG possède donc $C_{D1}$, $C_{D2}$, qu'il a fourni à D1 et D2.

* D1 et D2 possèdent en plus de ça un secret de flotte $S_{F}$. Ce secret est donné lors de la programmation de l'appareil par le partenaire (fabriquant/gouvernement, ici l'état de Genève). AG ne possède _pas_ ce secret. 

* D1 produit une mesure MA. Le contenu de cette mesure ne contient pas d'identification de D1 grâce à notre `MUID`. 
* D1 encrypte ce paquet grâce à $S_{F}$, puis envoie $E(C_{D1}, E(S_{F}, MA))$ à AG. 
* AG reçoit donc un payload $E(S_{F}, MA)$ qu'il ne peut pas décrypter, qu'il ajoute dans une table. 
* D2 demande cette table à AG, qu'il envoie sous forme $E(C_{D2}, [E(S_{F}, MA), E(S_{F}, Mn)])$. 
* D2 décrypte alors ce message et obtient $[MA, MB, ..., MN]$. Aucune de ces mesures n'est reliable à son génerateur. 
* D2 envoie maintenant $E(C_{D2}, [MA,MB,...,Mn])$ à AG. 
* AG reçoit alors une table de mesures.

On garantit alors que:

1. Toutes les données viennent d'un appareil autorisé par l'AG _ET_ le partenaire
2. Les données reçues en claire n'ont pas été produite que par le device à l'origine de la communication
3. D2 n'a pas été capable définir quel device avait produit la mesure MA
