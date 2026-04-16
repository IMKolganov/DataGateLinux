# -*- coding: utf-8 -*-
# Aligned with tools/extract_datagate_tr_strings.py (same length as extracted list).
TRANS = [
    {"ru": 'Сервер API временно недоступен. Попробуйте позже.', "fr": 'L’API est temporairement indisponible. Réessayez plus tard.', "el": 'Το API δεν είναι διαθέσιμο προσωρινά. Δοκιμάστε ξανά αργότερα.'},  # 0
    {"ru": 'Требуется Google Client ID.', "fr": 'Le Client ID Google est requis.', "el": 'Απαιτείται Google Client ID.'},  # 1
    {"ru": 'Недопустимый порт перенаправления OAuth.', "fr": 'Port de redirection OAuth invalide.', "el": 'Μη έγκυρη θύρα ανακατεύθυνσης OAuth.'},  # 2
    {"ru": 'Не удалось прослушать порт %1 для OAuth: %2. Закройте другой экземпляр DataGate или измените RedirectPort в appsettings и добавьте http://127.0.0.1:PORT/ в Google Cloud (как в Windows).', "fr": 'Impossible d’écouter le port %1 pour OAuth : %2. Fermez une autre instance DataGate ou modifiez RedirectPort dans appsettings et ajoutez http://127.0.0.1:PORT/ dans Google Cloud (comme sous Windows).', "el": 'Δεν ήταν δυνατή η ακρόαση στη θύρα %1 για OAuth: %2. Κλείστε άλλο στιγμιότυπο DataGate ή αλλάξτε RedirectPort στο appsettings και προσθέστε http://127.0.0.1:PORT/ στο Google Cloud (όπως στα Windows).'},  # 3
    {"ru": 'Время входа истекло или вкладка браузера закрыта. Нажмите «Войти через Google» снова.', "fr": 'La connexion a expiré ou l’onglet du navigateur a été fermé. Cliquez à nouveau sur Se connecter avec Google.', "el": 'Η σύνδεση έληξε ή το tab του προγράμματος περιήγησης έκλεισε. Κάντε ξανά κλικ στη Σύνδεση με Google.'},  # 4
    {"ru": 'Неверный HTTP-запрос OAuth.', "fr": 'Requête HTTP OAuth invalide.', "el": 'Μη έγκυρο αίτημα HTTP OAuth.'},  # 5
    {"ru": 'Неверная первая строка HTTP OAuth.', "fr": 'Ligne de requête HTTP OAuth invalide.', "el": 'Μη έγκυρη γραμμή αιτήματος HTTP OAuth.'},  # 6
    {"ru": 'Google OAuth: %1 %2', "fr": 'Google OAuth : %1 %2', "el": 'Google OAuth: %1 %2'},  # 7
    {"ru": 'Несовпадение состояния OAuth.', "fr": 'État OAuth incohérent.', "el": 'Αναντιστοιχία κατάστασης OAuth.'},  # 8
    {"ru": 'Код авторизации не получен.', "fr": 'Code d’autorisation non reçu.', "el": 'Δεν ελήφθη κωδικός εξουσιοδότησης.'},  # 9
    {"ru": 'Вход в API: %1 %2', "fr": 'Connexion API : %1 %2', "el": 'Σύνδεση API: %1 %2'},  # 10
    {"ru": 'Некорректный JSON от API.', "fr": 'JSON invalide provenant de l’API.', "el": 'Μη έγκυρο JSON από το API.'},  # 11
    {"ru": 'В ответе API нет токена.', "fr": 'Aucun jeton dans la réponse API.', "el": 'Δεν υπάρχει διακριτικό στην απάντηση API.'},  # 12
    {"ru": 'Не удалось открыть браузер.', "fr": 'Impossible d’ouvrir le navigateur.', "el": 'Δεν ήταν δυνατό το άνοιγμα του προγράμματος περιήγησης.'},  # 13
    {"ru": 'DataGate — вход', "fr": 'DataGate — Connexion', "el": 'DataGate — Σύνδεση'},  # 14
    {"ru": 'Добро пожаловать в DataGate', "fr": 'Bienvenue dans DataGate', "el": 'Καλώς ήρθατε στο DataGate'},  # 15
    {"ru": 'Войдите с аккаунтом Google, чтобы продолжить.', "fr": 'Connectez-vous avec votre compte Google pour continuer.', "el": 'Συνδεθείτε με τον λογαριασμό Google σας για να συνεχίσετε.'},  # 16
    {"ru": 'Отмена', "fr": 'Annuler', "el": 'Άκυρο'},  # 17
    {"ru": 'Войти через Google', "fr": 'Se connecter avec Google', "el": 'Σύνδεση με Google'},  # 18
    {"ru": 'DataGate', "fr": 'DataGate', "el": 'DataGate'},  # 19
    {"ru": 'Укажите Api:BaseUrl и GoogleAuth:ClientId в appsettings.json.', "fr": 'Configurez Api:BaseUrl et GoogleAuth:ClientId dans appsettings.json.', "el": 'Ορίστε Api:BaseUrl και GoogleAuth:ClientId στο appsettings.json.'},  # 20
    {"ru": 'Ожидание входа в браузере…', "fr": 'En attente de la connexion dans le navigateur…', "el": 'Αναμονή σύνδεσης στο πρόγραμμα περιήγησης…'},  # 21
    {"ru": 'DataGate OpenVPN 3', "fr": 'DataGate OpenVPN 3', "el": 'DataGate OpenVPN 3'},  # 22
    {"ru": 'Главная', "fr": 'Accueil', "el": 'Αρχική'},  # 23
    {"ru": 'Доступ', "fr": 'Accès', "el": 'Πρόσβαση'},  # 24
    {"ru": 'Статистика', "fr": 'Statistiques', "el": 'Στατιστικά'},  # 25
    {"ru": 'Настройки', "fr": 'Paramètres', "el": 'Ρυθμίσεις'},  # 26
    {"ru": 'Добро пожаловать в DataGate OpenVPN 3', "fr": 'Bienvenue dans DataGate OpenVPN 3', "el": 'Καλώς ήρθατε στο DataGate OpenVPN 3'},  # 27
    {"ru": 'Состояние подключения', "fr": 'État de la connexion', "el": 'Κατάσταση σύνδεσης'},  # 28
    {"ru": 'Ожидание', "fr": 'Inactif', "el": 'Αδρανής'},  # 29
    {"ru": 'Подключено к: —', "fr": 'Connecté à : —', "el": 'Συνδεδεμένος σε: —'},  # 30
    {"ru": 'VPN-сервер:', "fr": 'Serveur VPN :', "el": 'Διακομιστής VPN:'},  # 31
    {"ru": 'Автоматически (лучший сервер)', "fr": 'Automatique (meilleur serveur)', "el": 'Αυτόματα (καλύτερος διακομιστής)'},  # 32
    {"ru": 'Выбрать сервер…', "fr": 'Choisir un serveur…', "el": 'Επιλογή διακομιστή…'},  # 33
    {"ru": 'Сервер:', "fr": 'Serveur :', "el": 'Διακομιστής:'},  # 34
    {"ru": 'Подключить', "fr": 'Connecter', "el": 'Σύνδεση'},  # 35
    {"ru": 'Журнал движка', "fr": 'Journaux du moteur', "el": 'Καταγραφές μηχανής'},  # 36
    {"ru": 'Обновить список серверов', "fr": 'Actualiser la liste des serveurs', "el": 'Ανανέωση λίστας διακομιστών'},  # 37
    {"ru": 'Сервер', "fr": 'Serveur', "el": 'Διακομιστής'},  # 38
    {"ru": 'Онлайн', "fr": 'En ligne', "el": 'Συνδεδεμένος'},  # 39
    {"ru": 'Клиенты', "fr": 'Clients', "el": 'Πελάτες'},  # 40
    {"ru": 'Всего клиентов: —', "fr": 'Total clients : —', "el": 'Σύνολο πελατών: —'},  # 41
    {"ru": 'Язык', "fr": 'Langue', "el": 'Γλώσσα'},  # 42
    {"ru": 'Язык интерфейса. Сохраните настройки; при необходимости перезапустите приложение, если подписи не обновились.', "fr": 'Langue de l’interface. Enregistrez les paramètres ; redémarrez l’application si certains libellés restent dans l’ancienne langue.', "el": 'Γλώσσα διεπαφής. Αποθηκεύστε τις ρυθμίσεις· επανεκκινήστε την εφαρμογή αν κάποιες ετικέτες παραμείνουν στην παλιά γλώσσα.'},  # 43
    {"ru": 'Оформление', "fr": 'Apparence', "el": 'Εμφάνιση'},  # 44
    {"ru": 'Выберите тему приложения.', "fr": 'Choisissez le thème de l’application.', "el": 'Επιλέξτε θέμα εφαρμογής.'},  # 45
    {"ru": 'Тема', "fr": 'Thème', "el": 'Θέμα'},  # 46
    {"ru": 'Тёмная тема', "fr": 'Mode sombre', "el": 'Σκοτεινό θέμα'},  # 47
    {"ru": 'openvpn', "fr": 'openvpn', "el": 'openvpn'},  # 48
    {"ru": 'Команда OpenVPN', "fr": 'Commande OpenVPN', "el": 'Εντολή OpenVPN'},  # 49
    {"ru": 'Туннель (TUN): в Linux TUN доступен только с CAP_NET_ADMIN у бинарника openvpn. Используйте «Выдать право TUN» или: sudo setcap cap_net_admin+ep $(command -v openvpn). По возможности не запускайте всё приложение от sudo.', "fr": 'Tunnel (TUN) : sous Linux, TUN n’est autorisé qu’avec CAP_NET_ADMIN sur le binaire openvpn. Utilisez Accorder la capacité TUN ou : sudo setcap cap_net_admin+ep $(command -v openvpn). Évitez de lancer toute l’application en sudo si possible.', "el": 'Σήραγγα (TUN): Στο Linux το TUN επιτρέπεται μόνο με CAP_NET_ADMIN στο δυαδικό openvpn. Χρησιμοποιήστε «Χορήγηση δυνατότητας TUN» ή: sudo setcap cap_net_admin+ep $(command -v openvpn). Αποφύγετε να τρέχετε ολόκληρη την εφαρμογή ως sudo αν είναι δυνατόν.'},  # 50
    {"ru": 'Выдать право TUN…', "fr": 'Accorder la capacité TUN…', "el": 'Χορήγηση δυνατότητας TUN…'},  # 51
    {"ru": 'Сохранить настройки', "fr": 'Enregistrer les paramètres', "el": 'Αποθήκευση ρυθμίσεων'},  # 52
    {"ru": 'Учётная запись', "fr": 'Compte', "el": 'Λογαριασμός'},  # 53
    {"ru": 'Выйти из приложения.', "fr": 'Se déconnecter de l’application.', "el": 'Αποσύνδεση από την εφαρμογή.'},  # 54
    {"ru": 'Выйти', "fr": 'Déconnexion', "el": 'Αποσύνδεση'},  # 55
    {"ru": 'Настройки сохранены.', "fr": 'Paramètres enregistrés.', "el": 'Οι ρυθμίσεις αποθηκεύτηκαν.'},  # 56
    {"ru": 'Подключение…', "fr": 'Connexion…', "el": 'Σύνδεση…'},  # 57
    {"ru": 'Отключение…', "fr": 'Déconnexion…', "el": 'Αποσύνδεση…'},  # 58
    {"ru": 'Отключить', "fr": 'Déconnecter', "el": 'Αποσύνδεση'},  # 59
    {"ru": 'В appsettings.json отсутствует Api:BaseUrl.', "fr": 'Api:BaseUrl est absent de appsettings.json.', "el": 'Λείπει το Api:BaseUrl στο appsettings.json.'},  # 60
    {"ru": 'Не удалось получить токен доступа. API может быть недоступен — попробуйте позже; либо сессия истекла — выйдите (Настройки) и войдите снова.', "fr": 'Impossible d’obtenir un jeton d’accès. L’API peut être indisponible — réessayez plus tard, ou la session a expiré — déconnectez-vous (Paramètres) et reconnectez-vous.', "el": 'Δεν ήταν δυνατή η λήψη διακριτικού πρόσβασης. Το API μπορεί να μην είναι διαθέσιμο — δοκιμάστε αργότερα, ή έληξε η συνεδρία — αποσυνδεθείτε (Ρυθμίσεις) και συνδεθείτε ξανά.'},  # 61
    {"ru": 'Выберите VPN-сервер или обновите список.', "fr": 'Choisissez un serveur VPN ou actualisez la liste.', "el": 'Επιλέξτε διακομιστή VPN ή ανανεώστε τη λίστα.'},  # 62
    {"ru": 'Ошибка: ', "fr": 'Erreur : ', "el": 'Σφάλμα: '},  # 63
    {"ru": 'Подключено к: %1', "fr": 'Connecté à : %1', "el": 'Συνδεδεμένος σε: %1'},  # 64
    {"ru": '—', "fr": '—', "el": '—'},  # 65
    {"ru": 'Вход выполнен как %1.', "fr": 'Connecté en tant que %1.', "el": 'Συνδεδεμένος ως %1.'},  # 66
    {"ru": 'Бинарник OpenVPN не найден: %1', "fr": 'Binaire OpenVPN introuvable : %1', "el": 'Δεν βρέθηκε το δυαδικό OpenVPN: %1'},  # 67
    {"ru": 'Не является исполняемым файлом: %1', "fr": 'N’est pas un exécutable : %1', "el": 'Δεν είναι εκτελέσιμο: %1'},  # 68
    {"ru": 'setcap отклонён: путь должен содержать «openvpn».', "fr": 'setcap refusé : le chemin doit contenir « openvpn ».', "el": 'Απόρριψη setcap: η διαδρομή πρέπει να περιέχει «openvpn».'},  # 69
    {"ru": '\n', "fr": '\n', "el": '\n'},  # 70
    {"ru": 'Право выдано.\n%1\n\nПроверка: getcap %2', "fr": 'Capacité accordée.\n%1\n\nVérification : getcap %2', "el": 'Η δυνατότητα χορηγήθηκε.\n%1\n\nΕπαλήθευση: getcap %2'},  # 71
    {"ru": 'setcap cap_net_admin+ep на ', "fr": 'setcap cap_net_admin+ep sur ', "el": 'setcap cap_net_admin+ep στο '},  # 72
    {"ru": 'Ошибка pkexec/setcap (код %1).\n\n%2\n\nВручную:\nsudo setcap cap_net_admin+ep %3\ngetcap %3', "fr": 'Échec pkexec/setcap (code %1).\n\n%2\n\nManuel :\nsudo setcap cap_net_admin+ep %3\ngetcap %3', "el": 'Αποτυχία pkexec/setcap (έξοδος %1).\n\n%2\n\nΧειροκίνητα:\nsudo setcap cap_net_admin+ep %3\ngetcap %3'},  # 73
    {"ru": '(нет вывода)', "fr": '(aucune sortie)', "el": '(χωρίς έξοδο)'},  # 74
    {"ru": 'Только Linux.', "fr": 'Linux uniquement.', "el": 'Μόνο Linux.'},  # 75
    {"ru": 'Может потребоваться настройка TUN / прав OpenVPN.', "fr": 'Une configuration TUN / droits OpenVPN peut être nécessaire.', "el": 'Μπορεί να απαιτηθεί ρύθμιση TUN / δικαιωμάτων OpenVPN.'},  # 76
    {"ru": 'OpenVPN нуждается в CAP_NET_ADMIN у бинарника openvpn для создания TUN. DataGate не может назначить это само себе.', "fr": 'OpenVPN a besoin de CAP_NET_ADMIN sur le binaire openvpn pour créer un périphérique TUN. DataGate ne peut pas l’ajouter à lui-même.', "el": 'Το OpenVPN χρειάζεται CAP_NET_ADMIN στο δυαδικό openvpn για TUN. Το DataGate δεν μπορεί να το προσθέσει στον εαυτό του.'},  # 77
    {"ru": 'Открыть настройки', "fr": 'Ouvrir les paramètres', "el": 'Άνοιγμα ρυθμίσεων'},  # 78
    {"ru": 'Укажите Api:BaseUrl в appsettings.json.', "fr": 'Configurez Api:BaseUrl dans appsettings.json.', "el": 'Ορίστε Api:BaseUrl στο appsettings.json.'},  # 79
    {"ru": 'API недоступен — попробуйте позже. Если сеть в порядке, выйдите (Настройки) и войдите снова.', "fr": 'L’API est indisponible — réessayez plus tard. Si le réseau est correct, déconnectez-vous (Paramètres) et reconnectez-vous.', "el": 'Το API δεν είναι διαθέσιμο — δοκιμάστε αργότερα. Αν το δίκτυο είναι εντάξει, αποσυνδεθείτε (Ρυθμίσεις) και συνδεθείτε ξανά.'},  # 80
    {"ru": 'Тихое обновление списка серверов не удалось: %1', "fr": 'Actualisation silencieuse des serveurs échouée : %1', "el": 'Η σιωπηλή ανανέωση διακομιστών απέτυχε: %1'},  # 81
    {"ru": 'Некорректный JSON.', "fr": 'JSON invalide.', "el": 'Μη έγκυρο JSON.'},  # 82
    {"ru": 'да', "fr": 'oui', "el": 'ναι'},  # 83
    {"ru": 'нет', "fr": 'non', "el": 'όχι'},  # 84
    {"ru": 'Всего клиентов: %1', "fr": 'Total clients : %1', "el": 'Σύνολο πελατών: %1'},  # 85
    {"ru": 'Трафик (МБ) — по клиентам', "fr": 'Trafic (Mo) — par client', "el": 'Κίνηση (MB) — ανά πελάτη'},  # 86
    {"ru": 'Нет данных', "fr": 'Aucune donnée', "el": 'Δεν υπάρχουν δεδομένα'},  # 87
    {"ru": '%1 — %2 МБ', "fr": '%1 — %2 Mo', "el": '%1 — %2 MB'},  # 88
    {"ru": 'Трафик по серверу из API (GET open-vpn-statistics/get/{id}).', "fr": 'Trafic par serveur depuis l’API (GET open-vpn-statistics/get/{id}).', "el": 'Κίνηση ανά διακομιστή από το API (GET open-vpn-statistics/get/{id}).'},  # 89
    {"ru": 'Только мой трафик (JWT externalId)', "fr": 'Mon trafic uniquement (JWT externalId)', "el": 'Μόνο η κίνησή μου (JWT externalId)'},  # 90
    {"ru": 'Загрузить статистику', "fr": 'Charger les statistiques', "el": 'Φόρτωση στατιστικών'},  # 91
    {"ru": 'Сервер %1', "fr": 'Serveur %1', "el": 'Διακομιστής %1'},  # 92
    {"ru": 'Готово. Выберите сервер и нажмите «Загрузить статистику».', "fr": 'Prêt. Choisissez un serveur et appuyez sur Charger les statistiques.', "el": 'Έτοιμο. Επιλέξτε διακομιστή και πατήστε Φόρτωση στατιστικών.'},  # 93
    {"ru": 'Войдите и убедитесь, что задан Api:BaseUrl.', "fr": 'Connectez-vous et assurez-vous qu’Api:BaseUrl est défini.', "el": 'Συνδεθείτε και βεβαιωθείτε ότι έχει οριστεί Api:BaseUrl.'},  # 94
    {"ru": 'Выберите VPN-сервер (при пустом списке обновите на «Доступ»).', "fr": 'Choisissez un serveur VPN (si la liste est vide, actualisez depuis Accès).', "el": 'Επιλέξτε διακομιστή VPN (αν η λίστα είναι κενή, ανανεώστε από Πρόσβαση).'},  # 95
    {"ru": 'Загрузка…', "fr": 'Chargement…', "el": 'Φόρτωση…'},  # 96
    {"ru": 'Ошибка: %1', "fr": 'Erreur : %1', "el": 'Σφάλμα: %1'},  # 97
    {"ru": 'API: %1', "fr": 'API : %1', "el": 'API: %1'},  # 98
    {"ru": '(клиент)', "fr": '(client)', "el": '(πελάτης)'},  # 99
    {"ru": 'В ответе нет строк clientTraffics.', "fr": 'Aucune ligne clientTraffics dans la réponse.', "el": 'Δεν υπάρχουν γραμμές clientTraffics στην απάντηση.'},  # 100
    {"ru": 'Нет строк для вашего externalId в этом ответе.', "fr": 'Aucune ligne pour votre externalId dans cette réponse.', "el": 'Δεν υπάρχουν γραμμές για το externalId σας σε αυτή την απάντηση.'},  # 101
    {"ru": 'Загружено строк: %1.', "fr": 'Chargé : %1 ligne(s).', "el": 'Φορτώθηκαν %1 γραμμή(ές).'},  # 102
    {"ru": 'Сначала отключите текущую сессию OpenVPN.', "fr": 'Déconnectez d’abord la session OpenVPN en cours.', "el": 'Αποσυνδέστε πρώτα την τρέχουσα συνεδρία OpenVPN.'},  # 103
    {"ru": 'Подключение уже выполняется.', "fr": 'Connexion déjà en cours.', "el": 'Η σύνδεση βρίσκεται ήδη σε εξέλιξη.'},  # 104
    {"ru": 'Не удалось прочитать externalId из JWT (нужны sub, externalId или nameid).', "fr": 'Impossible de lire externalId depuis le JWT (sub, externalId ou nameid requis).', "el": 'Δεν ήταν δυνατή η ανάγνωση externalId από JWT (χρειάζονται sub, externalId ή nameid).'},  # 105
    {"ru": 'Запрос списка серверов…', "fr": 'Demande de la liste des serveurs…', "el": 'Αίτηση λίστας διακομιστών…'},  # 106
    {"ru": 'Серверы: %1', "fr": 'Serveurs : %1', "el": 'Διακομιστές: %1'},  # 107
    {"ru": 'Сервер с id %1 не найден или без WSS (обновите список на «Доступ»).', "fr": 'Serveur id %1 introuvable ou non compatible WSS (actualisez la liste dans Accès).', "el": 'Ο διακομιστής id %1 δεν βρέθηκε ή δεν έχει WSS (ανανεώστε τη λίστα στην Πρόσβαση).'},  # 108
    {"ru": 'Нет доступных серверов с WSS.', "fr": 'Aucun serveur compatible WSS disponible.', "el": 'Δεν υπάρχουν διαθέσιμοι διακομιστές με WSS.'},  # 109
    {"ru": 'Выбран сервер: %1', "fr": 'Serveur sélectionné : %1', "el": 'Επιλεγμένος διακομιστής: %1'},  # 110
    {"ru": 'Загрузка профиля OpenVPN…', "fr": 'Téléchargement du profil OpenVPN…', "el": 'Λήψη προφίλ OpenVPN…'},  # 111
    {"ru": 'Скачивание .ovpn: %1', "fr": 'Téléchargement .ovpn : %1', "el": 'Λήψη .ovpn: %1'},  # 112
    {"ru": 'Некорректный JSON при загрузке профиля.', "fr": 'JSON invalide lors du chargement du profil.', "el": 'Μη έγκυρο JSON κατά τη φόρτωση προφίλ.'},  # 113
    {"ru": 'В ответе нет содержимого (base64).', "fr": 'La réponse ne contient pas de contenu (base64).', "el": 'Η απάντηση δεν έχει περιεχόμενο (base64).'},  # 114
    {"ru": 'Ошибка загрузки профиля (HTTP %1). %2', "fr": 'Échec du téléchargement du profil (HTTP %1). %2', "el": 'Αποτυχία λήψης προφίλ (HTTP %1). %2'},  # 115
    {"ru": 'Создание профиля на сервере…', "fr": 'Création du profil sur le serveur…', "el": 'Δημιουργία προφίλ στον διακομιστή…'},  # 116
    {"ru": 'Создание профиля: HTTP %1 %2', "fr": 'Création du profil : HTTP %1 %2', "el": 'Δημιουργία προφίλ: HTTP %1 %2'},  # 117
    {"ru": 'Некорректный apiUrl сервера.', "fr": 'apiUrl du serveur invalide.', "el": 'Μη έγκυρο apiUrl διακομιστή.'},  # 118
    {"ru": 'Не удалось занять локальный порт %1 для моста WSS. Другой процесс может его удерживать (например «осиротевший» openvpn). В Linux: fuser -k %1/udp ; fuser -k %1/tcp', "fr": 'Impossible de lier le port local %1 pour le pont WSS. Un autre processus peut encore le détenir (openvpn orphelin). Sous Linux : fuser -k %1/udp ; fuser -k %1/tcp', "el": 'Δεν ήταν δυνατή η δέσμευση τοπικής θύρας %1 για τη γέφυρα WSS. Άλλη διεργασία μπορεί να την κατέχει. Σε Linux: fuser -k %1/udp ; fuser -k %1/tcp'},  # 119
    {"ru": 'Не удалось создать временный файл конфигурации.', "fr": 'Impossible de créer le fichier de configuration temporaire.', "el": 'Δεν ήταν δυνατή η δημιουργία προσωρινού αρχείου ρυθμίσεων.'},  # 120
    {"ru": 'Мост UDP↔WSS на порту %1…', "fr": 'Pont UDP↔WSS sur le port %1…', "el": 'Γέφυρα UDP↔WSS στη θύρα %1…'},  # 121
    {"ru": 'Мост TCP↔WSS на порту %1…', "fr": 'Pont TCP↔WSS sur le port %1…', "el": 'Γέφυρα TCP↔WSS στη θύρα %1…'},  # 122
    {"ru": 'Запуск OpenVPN…', "fr": 'Démarrage d’OpenVPN…', "el": 'Εκκίνηση OpenVPN…'},  # 123
    {"ru": 'OpenVPN не запустился за 8 с (%1). Выдайте CAP_NET_ADMIN бинарнику openvpn, если TUN блокируется.', "fr": 'OpenVPN n’a pas démarré en 8 s (%1). Accordez CAP_NET_ADMIN au binaire openvpn si TUN est bloqué.', "el": 'Το OpenVPN δεν ξεκίνησε εντός 8 δ (%1). Χορηγήστε CAP_NET_ADMIN στο δυαδικό openvpn αν αποκλείεται το TUN.'},  # 124
    {"ru": 'OpenVPN запущен.', "fr": 'OpenVPN a démarré.', "el": 'Το OpenVPN ξεκίνησε.'},  # 125
    {"ru": 'IgnoreRedirectGateway включён: push маршрута по умолчанию с сервера игнорируется, публичный IP обычно не меняется, туннель используется только для VPN-маршрутов. Установите OpenVpn.IgnoreRedirectGateway в false в appsettings.json, если нужен полный туннель (весь трафик и публичный IP через VPN).', "fr": 'IgnoreRedirectGateway est activé : la route par défaut poussée par le serveur est ignorée, votre IP publique reste en principe la même et seuls les itinéraires VPN passent par le tunnel. Mettez OpenVpn.IgnoreRedirectGateway à false dans appsettings.json si vous voulez un tunnel complet (tout le trafic et l’IP publique via le VPN).', "el": 'Το IgnoreRedirectGateway είναι ενεργό: αγνοείται η προώθηση προεπιλεγμένης διαδρομής, η δημόσια IP συνήθως παραμένει ίδια και μόνο VPN-διαδρομές χρησιμοποιούν τη σήραγγα. Ορίστε OpenVpn.IgnoreRedirectGateway σε false στο appsettings.json αν χρειάζεστε πλήρη σήραγγα (όλη η κίνηση και η δημόσια IP μέσω VPN).'},  # 126
    {"ru": 'OpenVPN завершился (код %1).', "fr": 'OpenVPN s’est terminé (code %1).', "el": 'Το OpenVPN τερματίστηκε (κωδικός %1).'},  # 127
    {"ru": 'OpenVPN завершился с ошибкой (%1), вывода нет. В Linux для TUN обычно нужен CAP_NET_ADMIN у openvpn (Настройки → Выдать право TUN, или sudo setcap cap_net_admin+ep /путь/к/openvpn).', "fr": 'OpenVPN a échoué (code %1) sans sortie capturée. Sous Linux, TUN nécessite en général CAP_NET_ADMIN sur openvpn (Paramètres → Accorder TUN, ou sudo setcap cap_net_admin+ep /chemin/vers/openvpn).', "el": 'Το OpenVPN απέτυχε (έξοδος %1) χωρίς καταγεγραμμένη έξοδο. Στο Linux, το TUN συνήθως χρειάζεται CAP_NET_ADMIN στο openvpn (Ρυθμίσεις → Χορήγηση TUN, ή sudo setcap cap_net_admin+ep /διαδρομή/στο/openvpn).'},  # 128
    {"ru": 'Создание TUN отклонено (нужен CAP_NET_ADMIN у процесса openvpn, не у GUI).\n\nOpenVPN завершился с ошибкой (%1):\n%2', "fr": 'Création TUN refusée (CAP_NET_ADMIN requis sur le processus openvpn, pas sur l’interface).\n\nOpenVPN a échoué (code %1) :\n%2', "el": 'Η δημιουργία TUN απορρίφθηκε (CAP_NET_ADMIN απαιτείται στη διεργασία openvpn, όχι στο GUI).\n\nΤο OpenVPN απέτυχε (έξοδος %1):\n%2'},  # 129
    {"ru": 'Ошибка OpenVPN (код %1):\n%2', "fr": 'Échec OpenVPN (code %1) :\n%2', "el": 'Αποτυχία OpenVPN (έξοδος %1):\n%2'},  # 130
    {"ru": 'Ошибка процесса OpenVPN: %1', "fr": 'Erreur du processus OpenVPN : %1', "el": 'Σφάλμα διεργασίας OpenVPN: %1'},  # 131
    {"ru": 'Файл appsettings.json не найден (рабочая каталог, папка с exe или путь AppConfig), либо пусты Api:BaseUrl / GoogleAuth:ClientId.\nСм. строку в журнале «AppConfig: loaded …» при DATAGATE_LOG=1 или сборке Debug.', "fr": 'appsettings.json introuvable (répertoire courant, dossier de l’exe ou chemin AppConfig), ou Api:BaseUrl / GoogleAuth:ClientId vides.\nVoir la ligne « AppConfig: loaded … » sur stderr avec DATAGATE_LOG=1 ou build Debug.', "el": 'Δεν βρέθηκε appsettings.json (cwd, φάκελος exe ή διαδρομή AppConfig), ή κενά Api:BaseUrl / GoogleAuth:ClientId.\nΔείτε stderr «AppConfig: loaded …» με DATAGATE_LOG=1 ή Debug build.'},  # 132
]
