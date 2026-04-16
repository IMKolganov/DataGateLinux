# -*- coding: utf-8 -*-
# Aligned with tools/extract_datagate_tr_strings.py (same length as extracted list).
# Regenerate: python3 tools/refresh_translations_index.py
TRANS = [
    {"ru": 'Сервер API временно недоступен. Попробуйте позже.', "fr": 'L’API est temporairement indisponible. Réessayez plus tard.', "el": 'Το API δεν είναι διαθέσιμο προσωρινά. Δοκιμάστε ξανά αργότερα.'},  # 0 The API is temporarily unavailable. Try again later.
    {"ru": 'Вы вошли', "fr": 'Vous êtes connecté(e).', "el": 'Έχετε συνδεθεί.'},  # 1 You're signed in
    {"ru": 'Можно закрыть эту вкладку и вернуться в DataGate.', "fr": 'Vous pouvez fermer cet onglet et revenir à DataGate.', "el": 'Μπορείτε να κλείσετε αυτή την καρτέλα και να επιστρέψετε στο DataGate.'},  # 2 You can close this tab and return to the DataGate app.
    {"ru": 'Вход не завершён', "fr": 'La connexion n’a pas abouti.', "el": 'Η σύνδεση δεν ολοκληρώθηκε.'},  # 3 Sign-in did not complete
    {"ru": 'Закройте вкладку и попробуйте снова из DataGate.', "fr": 'Fermez cet onglet et réessayez depuis DataGate.', "el": 'Κλείστε αυτή την καρτέλα και δοκιμάστε ξανά από το DataGate.'},  # 4 You can close this tab and try again from DataGate.
    {"ru": 'Некорректный запрос входа', "fr": 'Requête de connexion invalide.', "el": 'Μη έγκυρο αίτημα σύνδεσης.'},  # 5 Invalid sign-in request
    {"ru": 'Закройте вкладку и начните вход снова из DataGate.', "fr": 'Fermez cet onglet et recommencez la connexion depuis DataGate.', "el": 'Κλείστε αυτή την καρτέλα και ξεκινήστε ξανά τη σύνδεση από το DataGate.'},  # 6 Close this tab and start sign-in again from DataGate.
    {"ru": 'Требуется Google Client ID.', "fr": 'Le Client ID Google est requis.', "el": 'Απαιτείται Google Client ID.'},  # 7 Google Client ID is required.
    {"ru": 'Недопустимый порт перенаправления OAuth.', "fr": 'Port de redirection OAuth invalide.', "el": 'Μη έγκυρη θύρα ανακατεύθυνσης OAuth.'},  # 8 Invalid OAuth redirect port.
    {"ru": 'Не удалось прослушать порт %1 для OAuth: %2. Закройте другой экземпляр DataGate или измените RedirectPort в appsettings и добавьте http://127.0.0.1:PORT/ в Google Cloud (как в Windows).', "fr": 'Impossible d’écouter le port %1 pour OAuth : %2. Fermez une autre instance DataGate ou modifiez RedirectPort dans appsettings et ajoutez http://127.0.0.1:PORT/ dans Google Cloud (comme sous Windows).', "el": 'Δεν ήταν δυνατή η ακρόαση στη θύρα %1 για OAuth: %2. Κλείστε άλλο στιγμιότυπο DataGate ή αλλάξτε RedirectPort στο appsettings και προσθέστε http://127.0.0.1:PORT/ στο Google Cloud (όπως στα Windows).'},  # 9 Could not listen on port %1 for OAuth: %2. Close another
    {"ru": 'Время входа истекло или вкладка браузера закрыта. Нажмите «Войти через Google» снова.', "fr": 'La connexion a expiré ou l’onglet du navigateur a été fermé. Cliquez à nouveau sur Se connecter avec Google.', "el": 'Η σύνδεση έληξε ή το tab του προγράμματος περιήγησης έκλεισε. Κάντε ξανά κλικ στη Σύνδεση με Google.'},  # 10 Sign-in timed out or the browser tab was closed. Click S
    {"ru": 'Неверный HTTP-запрос OAuth.', "fr": 'Requête HTTP OAuth invalide.', "el": 'Μη έγκυρο αίτημα HTTP OAuth.'},  # 11 Invalid OAuth HTTP request.
    {"ru": 'Неверная первая строка HTTP OAuth.', "fr": 'Ligne de requête HTTP OAuth invalide.', "el": 'Μη έγκυρη γραμμή αιτήματος HTTP OAuth.'},  # 12 Invalid OAuth HTTP request line.
    {"ru": 'Google OAuth: %1 %2', "fr": 'Google OAuth : %1 %2', "el": 'Google OAuth: %1 %2'},  # 13 Google OAuth: %1 %2
    {"ru": 'Несовпадение состояния OAuth.', "fr": 'État OAuth incohérent.', "el": 'Αναντιστοιχία κατάστασης OAuth.'},  # 14 OAuth state mismatch.
    {"ru": 'Код авторизации не получен.', "fr": 'Code d’autorisation non reçu.', "el": 'Δεν ελήφθη κωδικός εξουσιοδότησης.'},  # 15 Authorization code not received.
    {"ru": 'Вход в API: %1 %2', "fr": 'Connexion API : %1 %2', "el": 'Σύνδεση API: %1 %2'},  # 16 API login: %1 %2
    {"ru": 'Некорректный JSON от API.', "fr": 'JSON invalide provenant de l’API.', "el": 'Μη έγκυρο JSON από το API.'},  # 17 Invalid JSON from API.
    {"ru": 'В ответе API нет токена.', "fr": 'Aucun jeton dans la réponse API.', "el": 'Δεν υπάρχει διακριτικό στην απάντηση API.'},  # 18 No token in API response.
    {"ru": 'Не удалось открыть браузер.', "fr": 'Impossible d’ouvrir le navigateur.', "el": 'Δεν ήταν δυνατό το άνοιγμα του προγράμματος περιήγησης.'},  # 19 Could not open browser.
    {"ru": 'DataGate — вход', "fr": 'DataGate — Connexion', "el": 'DataGate — Σύνδεση'},  # 20 DataGate — Sign in
    {"ru": 'Добро пожаловать в DataGate', "fr": 'Bienvenue dans DataGate', "el": 'Καλώς ήρθατε στο DataGate'},  # 21 Welcome to DataGate
    {"ru": 'Войдите с аккаунтом Google, чтобы продолжить.', "fr": 'Connectez-vous avec votre compte Google pour continuer.', "el": 'Συνδεθείτε με τον λογαριασμό Google σας για να συνεχίσετε.'},  # 22 Sign in with your Google account to continue.
    {"ru": 'Отмена', "fr": 'Annuler', "el": 'Άκυρο'},  # 23 Cancel
    {"ru": 'Войти через Google', "fr": 'Se connecter avec Google', "el": 'Σύνδεση με Google'},  # 24 Sign in with Google
    {"ru": 'DataGate', "fr": 'DataGate', "el": 'DataGate'},  # 25 DataGate
    {"ru": 'Укажите Api:BaseUrl и GoogleAuth:ClientId в appsettings.json.', "fr": 'Configurez Api:BaseUrl et GoogleAuth:ClientId dans appsettings.json.', "el": 'Ορίστε Api:BaseUrl και GoogleAuth:ClientId στο appsettings.json.'},  # 26 Configure Api:BaseUrl and GoogleAuth:ClientId in appsett
    {"ru": 'Ожидание входа в браузере…', "fr": 'En attente de la connexion dans le navigateur…', "el": 'Αναμονή σύνδεσης στο πρόγραμμα περιήγησης…'},  # 27 Waiting for browser sign-in…
    {"ru": 'DataGate OpenVPN 3', "fr": 'DataGate OpenVPN 3', "el": 'DataGate OpenVPN 3'},  # 28 DataGate OpenVPN 3
    {"ru": 'Главная', "fr": 'Accueil', "el": 'Αρχική'},  # 29 Home
    {"ru": 'Доступ', "fr": 'Accès', "el": 'Πρόσβαση'},  # 30 Access
    {"ru": 'Статистика', "fr": 'Statistiques', "el": 'Στατιστικά'},  # 31 Statistics
    {"ru": 'Настройки', "fr": 'Paramètres', "el": 'Ρυθμίσεις'},  # 32 Settings
    {"ru": 'Добро пожаловать в DataGate OpenVPN 3', "fr": 'Bienvenue dans DataGate OpenVPN 3', "el": 'Καλώς ήρθατε στο DataGate OpenVPN 3'},  # 33 Welcome to DataGate OpenVPN 3
    {"ru": 'Состояние подключения', "fr": 'État de la connexion', "el": 'Κατάσταση σύνδεσης'},  # 34 Connection status
    {"ru": 'Ожидание', "fr": 'Inactif', "el": 'Αδρανής'},  # 35 Idle
    {"ru": 'Подключено к: —', "fr": 'Connecté à : —', "el": 'Συνδεδεμένος σε: —'},  # 36 Connected to: —
    {"ru": 'VPN-сервер:', "fr": 'Serveur VPN :', "el": 'Διακομιστής VPN:'},  # 37 VPN server:
    {"ru": 'Автоматически (лучший сервер)', "fr": 'Automatique (meilleur serveur)', "el": 'Αυτόματα (καλύτερος διακομιστής)'},  # 38 Automatic (best server)
    {"ru": 'Выбрать сервер…', "fr": 'Choisir un serveur…', "el": 'Επιλογή διακομιστή…'},  # 39 Choose server…
    {"ru": 'Сервер:', "fr": 'Serveur :', "el": 'Διακομιστής:'},  # 40 Server:
    {"ru": 'Подключить', "fr": 'Connecter', "el": 'Σύνδεση'},  # 41 Connect
    {"ru": 'Журнал движка', "fr": 'Journaux du moteur', "el": 'Καταγραφές μηχανής'},  # 42 Engine logs
    {"ru": 'Обновить список серверов', "fr": 'Actualiser la liste des serveurs', "el": 'Ανανέωση λίστας διακομιστών'},  # 43 Refresh server list
    {"ru": 'Сервер', "fr": 'Serveur', "el": 'Διακομιστής'},  # 44 Server
    {"ru": 'Онлайн', "fr": 'En ligne', "el": 'Συνδεδεμένος'},  # 45 Online
    {"ru": 'Клиенты', "fr": 'Clients', "el": 'Πελάτες'},  # 46 Clients
    {"ru": 'Всего клиентов: —', "fr": 'Total clients : —', "el": 'Σύνολο πελατών: —'},  # 47 Total clients: —
    {"ru": 'Язык', "fr": 'Langue', "el": 'Γλώσσα'},  # 48 Language
    {"ru": 'После выбора язык интерфейса меняется сразу, выбор сохраняется автоматически. Путь OpenVPN, тема и сервер — кнопкой «Сохранить настройки».', "fr": 'La langue de l’interface se met à jour dès le choix et est enregistrée automatiquement. Utilisez Enregistrer les paramètres pour le chemin OpenVPN, le thème et le serveur.', "el": 'Η γλώσσα της διεπαφής ενημερώνεται αμέσως και αποθηκεύεται αυτόματα. Για διαδρομή OpenVPN, θέμα και διακομιστή χρησιμοποιήστε Αποθήκευση ρυθμίσεων.'},  # 49 Choosing a language updates the interface immediately
    {"ru": 'Оформление', "fr": 'Apparence', "el": 'Εμφάνιση'},  # 50 Appearance
    {"ru": 'Выберите тему приложения.', "fr": 'Choisissez le thème de l’application.', "el": 'Επιλέξτε θέμα εφαρμογής.'},  # 51 Choose the application theme.
    {"ru": 'Тема', "fr": 'Thème', "el": 'Θέμα'},  # 52 Theme
    {"ru": 'Тёмная тема', "fr": 'Mode sombre', "el": 'Σκοτεινό θέμα'},  # 53 Dark mode
    {"ru": 'Другие приложения', "fr": 'Autres applications', "el": 'Περισσότερες εφαρμογές'},  # 54 More apps
    {"ru": 'Другие приложения DataGate для ваших устройств можно скачать на сайте:', "fr": 'Vous pouvez télécharger les autres applications DataGate pour vos appareils sur le site :', "el": 'Μπορείτε να κατεβάσετε άλλες εφαρμογές DataGate για τις συσκευές σας από τον ιστότοπο:'},  # 55 You can download other DataGate apps for your devices fr
    {"ru": 'Windows', "fr": 'Windows', "el": 'Windows'},  # 56 Windows
    {"ru": 'Linux', "fr": 'Linux', "el": 'Linux'},  # 57 Linux
    {"ru": 'Android', "fr": 'Android', "el": 'Android'},  # 58 Android
    {"ru": 'iOS', "fr": 'iOS', "el": 'iOS'},  # 59 iOS
    {"ru": 'openvpn', "fr": 'openvpn', "el": 'openvpn'},  # 60 openvpn
    {"ru": 'Команда OpenVPN', "fr": 'Commande OpenVPN', "el": 'Εντολή OpenVPN'},  # 61 OpenVPN command
    {"ru": 'Туннель (TUN): в Linux TUN доступен только с CAP_NET_ADMIN у бинарника openvpn. Используйте «Выдать право TUN» или: sudo setcap cap_net_admin+ep $(command -v openvpn). По возможности не запускайте всё приложение от sudo.', "fr": 'Tunnel (TUN) : sous Linux, TUN n’est autorisé qu’avec CAP_NET_ADMIN sur le binaire openvpn. Utilisez Accorder la capacité TUN ou : sudo setcap cap_net_admin+ep $(command -v openvpn). Évitez de lancer toute l’application en sudo si possible.', "el": 'Σήραγγα (TUN): Στο Linux το TUN επιτρέπεται μόνο με CAP_NET_ADMIN στο δυαδικό openvpn. Χρησιμοποιήστε «Χορήγηση δυνατότητας TUN» ή: sudo setcap cap_net_admin+ep $(command -v openvpn). Αποφύγετε να τρέχετε ολόκληρη την εφαρμογή ως sudo αν είναι δυνατόν.'},  # 62 Tunnel (TUN): Linux allows TUN only with CAP_NET_ADMIN o
    {"ru": 'Выдать право TUN…', "fr": 'Accorder la capacité TUN…', "el": 'Χορήγηση δυνατότητας TUN…'},  # 63 Grant TUN capability…
    {"ru": 'Сохранить настройки', "fr": 'Enregistrer les paramètres', "el": 'Αποθήκευση ρυθμίσεων'},  # 64 Save settings
    {"ru": 'Учётная запись', "fr": 'Compte', "el": 'Λογαριασμός'},  # 65 Account
    {"ru": 'Выйти из приложения.', "fr": 'Se déconnecter de l’application.', "el": 'Αποσύνδεση από την εφαρμογή.'},  # 66 Sign out from the application.
    {"ru": 'Выйти', "fr": 'Déconnexion', "el": 'Αποσύνδεση'},  # 67 Logout
    {"ru": 'Настройки сохранены.', "fr": 'Paramètres enregistrés.', "el": 'Οι ρυθμίσεις αποθηκεύτηκαν.'},  # 68 Settings saved.
    {"ru": 'Подключение…', "fr": 'Connexion…', "el": 'Σύνδεση…'},  # 69 Connecting…
    {"ru": 'Отключение…', "fr": 'Déconnexion…', "el": 'Αποσύνδεση…'},  # 70 Disconnecting…
    {"ru": 'Отключить', "fr": 'Déconnecter', "el": 'Αποσύνδεση'},  # 71 Disconnect
    {"ru": 'В appsettings.json отсутствует Api:BaseUrl.', "fr": 'Api:BaseUrl est absent de appsettings.json.', "el": 'Λείπει το Api:BaseUrl στο appsettings.json.'},  # 72 Api:BaseUrl is missing in appsettings.json.
    {"ru": 'Не удалось получить токен доступа. API может быть недоступен — попробуйте позже; либо сессия истекла — выйдите (Настройки) и войдите снова.', "fr": 'Impossible d’obtenir un jeton d’accès. L’API peut être indisponible — réessayez plus tard, ou la session a expiré — déconnectez-vous (Paramètres) et reconnectez-vous.', "el": 'Δεν ήταν δυνατή η λήψη διακριτικού πρόσβασης. Το API μπορεί να μην είναι διαθέσιμο — δοκιμάστε αργότερα, ή έληξε η συνεδρία — αποσυνδεθείτε (Ρυθμίσεις) και συνδεθείτε ξανά.'},  # 73 Could not obtain an access token. The API may be unavail
    {"ru": 'Выберите VPN-сервер или обновите список.', "fr": 'Choisissez un serveur VPN ou actualisez la liste.', "el": 'Επιλέξτε διακομιστή VPN ή ανανεώστε τη λίστα.'},  # 74 Choose a VPN server or refresh the list.
    {"ru": 'Ошибка: ', "fr": 'Erreur : ', "el": 'Σφάλμα: '},  # 75 Error: 
    {"ru": 'Подключено к: %1', "fr": 'Connecté à : %1', "el": 'Συνδεδεμένος σε: %1'},  # 76 Connected to: %1
    {"ru": '—', "fr": '—', "el": '—'},  # 77 —
    {"ru": 'Вход выполнен как %1.', "fr": 'Connecté en tant que %1.', "el": 'Συνδεδεμένος ως %1.'},  # 78 Signed in as %1.
    {"ru": 'Бинарник OpenVPN не найден: %1', "fr": 'Binaire OpenVPN introuvable : %1', "el": 'Δεν βρέθηκε το δυαδικό OpenVPN: %1'},  # 79 OpenVPN binary not found: %1
    {"ru": 'Не является исполняемым файлом: %1', "fr": 'N’est pas un exécutable : %1', "el": 'Δεν είναι εκτελέσιμο: %1'},  # 80 Not an executable: %1
    {"ru": 'setcap отклонён: путь должен содержать «openvpn».', "fr": 'setcap refusé : le chemin doit contenir « openvpn ».', "el": 'Απόρριψη setcap: η διαδρομή πρέπει να περιέχει «openvpn».'},  # 81 Refusing setcap: path must contain “openvpn”.
    {"ru": '\n', "fr": '\n', "el": '\n'},  # 82  
    {"ru": 'Право выдано.\n%1\n\nПроверка: getcap %2', "fr": 'Capacité accordée.\n%1\n\nVérification : getcap %2', "el": 'Η δυνατότητα χορηγήθηκε.\n%1\n\nΕπαλήθευση: getcap %2'},  # 83 Capability granted. %1  Verify: getcap %2
    {"ru": 'setcap cap_net_admin+ep на ', "fr": 'setcap cap_net_admin+ep sur ', "el": 'setcap cap_net_admin+ep στο '},  # 84 setcap cap_net_admin+ep on 
    {"ru": 'Ошибка pkexec/setcap (код %1).\n\n%2\n\nВручную:\nsudo setcap cap_net_admin+ep %3\ngetcap %3', "fr": 'Échec pkexec/setcap (code %1).\n\n%2\n\nManuel :\nsudo setcap cap_net_admin+ep %3\ngetcap %3', "el": 'Αποτυχία pkexec/setcap (έξοδος %1).\n\n%2\n\nΧειροκίνητα:\nsudo setcap cap_net_admin+ep %3\ngetcap %3'},  # 85 pkexec/setcap failed (exit %1).  %2  Manual: sudo setcap
    {"ru": '(нет вывода)', "fr": '(aucune sortie)', "el": '(χωρίς έξοδο)'},  # 86 (no output)
    {"ru": 'Только Linux.', "fr": 'Linux uniquement.', "el": 'Μόνο Linux.'},  # 87 Linux only.
    {"ru": 'Может потребоваться настройка TUN / прав OpenVPN.', "fr": 'Une configuration TUN / droits OpenVPN peut être nécessaire.', "el": 'Μπορεί να απαιτηθεί ρύθμιση TUN / δικαιωμάτων OpenVPN.'},  # 88 TUN / OpenVPN capability setup may be required.
    {"ru": 'OpenVPN нуждается в CAP_NET_ADMIN у бинарника openvpn для создания TUN. DataGate не может назначить это само себе.', "fr": 'OpenVPN a besoin de CAP_NET_ADMIN sur le binaire openvpn pour créer un périphérique TUN. DataGate ne peut pas l’ajouter à lui-même.', "el": 'Το OpenVPN χρειάζεται CAP_NET_ADMIN στο δυαδικό openvpn για TUN. Το DataGate δεν μπορεί να το προσθέσει στον εαυτό του.'},  # 89 OpenVPN needs CAP_NET_ADMIN on the openvpn binary to cre
    {"ru": 'Открыть настройки', "fr": 'Ouvrir les paramètres', "el": 'Άνοιγμα ρυθμίσεων'},  # 90 Open Settings
    {"ru": 'Укажите Api:BaseUrl в appsettings.json.', "fr": 'Configurez Api:BaseUrl dans appsettings.json.', "el": 'Ορίστε Api:BaseUrl στο appsettings.json.'},  # 91 Configure Api:BaseUrl in appsettings.json.
    {"ru": 'API недоступен — попробуйте позже. Если сеть в порядке, выйдите (Настройки) и войдите снова.', "fr": 'L’API est indisponible — réessayez plus tard. Si le réseau est correct, déconnectez-vous (Paramètres) et reconnectez-vous.', "el": 'Το API δεν είναι διαθέσιμο — δοκιμάστε αργότερα. Αν το δίκτυο είναι εντάξει, αποσυνδεθείτε (Ρυθμίσεις) και συνδεθείτε ξανά.'},  # 92 The API is unavailable — try again later. If your networ
    {"ru": 'Тихое обновление списка серверов не удалось: %1', "fr": 'Actualisation silencieuse des serveurs échouée : %1', "el": 'Η σιωπηλή ανανέωση διακομιστών απέτυχε: %1'},  # 93 Silent server refresh failed: %1
    {"ru": 'Некорректный JSON.', "fr": 'JSON invalide.', "el": 'Μη έγκυρο JSON.'},  # 94 Invalid JSON.
    {"ru": 'да', "fr": 'oui', "el": 'ναι'},  # 95 yes
    {"ru": 'нет', "fr": 'non', "el": 'όχι'},  # 96 no
    {"ru": 'Всего клиентов: %1', "fr": 'Total clients : %1', "el": 'Σύνολο πελατών: %1'},  # 97 Total clients: %1
    {"ru": 'Трафик (МБ) — по клиентам', "fr": 'Trafic (Mo) — par client', "el": 'Κίνηση (MB) — ανά πελάτη'},  # 98 Traffic (MB) — per client
    {"ru": 'Нет данных', "fr": 'Aucune donnée', "el": 'Δεν υπάρχουν δεδομένα'},  # 99 No data
    {"ru": '%1 — %2 МБ', "fr": '%1 — %2 Mo', "el": '%1 — %2 MB'},  # 100 %1 — %2 MB
    {"ru": 'Трафик по серверу из API (GET open-vpn-statistics/get/{id}).', "fr": 'Trafic par serveur depuis l’API (GET open-vpn-statistics/get/{id}).', "el": 'Κίνηση ανά διακομιστή από το API (GET open-vpn-statistics/get/{id}).'},  # 101 Per-server traffic from the API (GET open-vpn-statistics
    {"ru": 'Только мой трафик (JWT externalId)', "fr": 'Mon trafic uniquement (JWT externalId)', "el": 'Μόνο η κίνησή μου (JWT externalId)'},  # 102 Only my traffic (JWT externalId)
    {"ru": 'Загрузить статистику', "fr": 'Charger les statistiques', "el": 'Φόρτωση στατιστικών'},  # 103 Load statistics
    {"ru": 'Сервер %1', "fr": 'Serveur %1', "el": 'Διακομιστής %1'},  # 104 Server %1
    {"ru": 'Готово. Выберите сервер и нажмите «Загрузить статистику».', "fr": 'Prêt. Choisissez un serveur et appuyez sur Charger les statistiques.', "el": 'Έτοιμο. Επιλέξτε διακομιστή και πατήστε Φόρτωση στατιστικών.'},  # 105 Ready. Choose a server and tap Load statistics.
    {"ru": 'Войдите и убедитесь, что задан Api:BaseUrl.', "fr": 'Connectez-vous et assurez-vous qu’Api:BaseUrl est défini.', "el": 'Συνδεθείτε και βεβαιωθείτε ότι έχει οριστεί Api:BaseUrl.'},  # 106 Sign in and ensure Api:BaseUrl is set.
    {"ru": 'Выберите VPN-сервер (при пустом списке обновите на «Доступ»).', "fr": 'Choisissez un serveur VPN (si la liste est vide, actualisez depuis Accès).', "el": 'Επιλέξτε διακομιστή VPN (αν η λίστα είναι κενή, ανανεώστε από Πρόσβαση).'},  # 107 Choose a VPN server (refresh list on Access if empty).
    {"ru": 'Загрузка…', "fr": 'Chargement…', "el": 'Φόρτωση…'},  # 108 Loading…
    {"ru": 'Ошибка: %1', "fr": 'Erreur : %1', "el": 'Σφάλμα: %1'},  # 109 Error: %1
    {"ru": 'API: %1', "fr": 'API : %1', "el": 'API: %1'},  # 110 API: %1
    {"ru": '(клиент)', "fr": '(client)', "el": '(πελάτης)'},  # 111 (client)
    {"ru": 'В ответе нет строк clientTraffics.', "fr": 'Aucune ligne clientTraffics dans la réponse.', "el": 'Δεν υπάρχουν γραμμές clientTraffics στην απάντηση.'},  # 112 No clientTraffics rows in the response.
    {"ru": 'Нет строк для вашего externalId в этом ответе.', "fr": 'Aucune ligne pour votre externalId dans cette réponse.', "el": 'Δεν υπάρχουν γραμμές για το externalId σας σε αυτή την απάντηση.'},  # 113 No rows for your externalId in this response.
    {"ru": 'Загружено строк: %1.', "fr": 'Chargé : %1 ligne(s).', "el": 'Φορτώθηκαν %1 γραμμή(ές).'},  # 114 Loaded %1 row(s).
    {"ru": 'Сначала отключите текущую сессию OpenVPN.', "fr": 'Déconnectez d’abord la session OpenVPN en cours.', "el": 'Αποσυνδέστε πρώτα την τρέχουσα συνεδρία OpenVPN.'},  # 115 Disconnect the current OpenVPN session first.
    {"ru": 'Подключение уже выполняется.', "fr": 'Connexion déjà en cours.', "el": 'Η σύνδεση βρίσκεται ήδη σε εξέλιξη.'},  # 116 Connection already in progress.
    {"ru": 'Не удалось прочитать externalId из JWT (нужны sub, externalId или nameid).', "fr": 'Impossible de lire externalId depuis le JWT (sub, externalId ou nameid requis).', "el": 'Δεν ήταν δυνατή η ανάγνωση externalId από JWT (χρειάζονται sub, externalId ή nameid).'},  # 117 Could not read externalId from JWT (need sub, externalId
    {"ru": 'Запрос списка серверов…', "fr": 'Demande de la liste des serveurs…', "el": 'Αίτηση λίστας διακομιστών…'},  # 118 Requesting server list…
    {"ru": 'Серверы: %1', "fr": 'Serveurs : %1', "el": 'Διακομιστές: %1'},  # 119 Servers: %1
    {"ru": 'Сервер с id %1 не найден или без WSS (обновите список на «Доступ»).', "fr": 'Serveur id %1 introuvable ou non compatible WSS (actualisez la liste dans Accès).', "el": 'Ο διακομιστής id %1 δεν βρέθηκε ή δεν έχει WSS (ανανεώστε τη λίστα στην Πρόσβαση).'},  # 120 Server id %1 not found or is not WSS-enabled (refresh th
    {"ru": 'Нет доступных серверов с WSS.', "fr": 'Aucun serveur compatible WSS disponible.', "el": 'Δεν υπάρχουν διαθέσιμοι διακομιστές με WSS.'},  # 121 No WSS-enabled servers available.
    {"ru": 'Выбран сервер: %1', "fr": 'Serveur sélectionné : %1', "el": 'Επιλεγμένος διακομιστής: %1'},  # 122 Selected server: %1
    {"ru": 'Загрузка профиля OpenVPN…', "fr": 'Téléchargement du profil OpenVPN…', "el": 'Λήψη προφίλ OpenVPN…'},  # 123 Downloading OpenVPN profile…
    {"ru": 'Скачивание .ovpn: %1', "fr": 'Téléchargement .ovpn : %1', "el": 'Λήψη .ovpn: %1'},  # 124 Download .ovpn: %1
    {"ru": 'Некорректный JSON при загрузке профиля.', "fr": 'JSON invalide lors du chargement du profil.', "el": 'Μη έγκυρο JSON κατά τη φόρτωση προφίλ.'},  # 125 Invalid JSON while loading profile.
    {"ru": 'В ответе нет содержимого (base64).', "fr": 'La réponse ne contient pas de contenu (base64).', "el": 'Η απάντηση δεν έχει περιεχόμενο (base64).'},  # 126 Response has no content (base64).
    {"ru": 'Ошибка загрузки профиля (HTTP %1). %2', "fr": 'Échec du téléchargement du profil (HTTP %1). %2', "el": 'Αποτυχία λήψης προφίλ (HTTP %1). %2'},  # 127 Profile download failed (HTTP %1). %2
    {"ru": 'Создание профиля на сервере…', "fr": 'Création du profil sur le serveur…', "el": 'Δημιουργία προφίλ στον διακομιστή…'},  # 128 Creating profile on server…
    {"ru": 'Создание профиля: HTTP %1 %2', "fr": 'Création du profil : HTTP %1 %2', "el": 'Δημιουργία προφίλ: HTTP %1 %2'},  # 129 Create profile: HTTP %1 %2
    {"ru": 'Некорректный apiUrl сервера.', "fr": 'apiUrl du serveur invalide.', "el": 'Μη έγκυρο apiUrl διακομιστή.'},  # 130 Invalid server apiUrl.
    {"ru": 'Не удалось занять локальный порт %1 для моста WSS. Другой процесс может его удерживать (например «осиротевший» openvpn). В Linux: fuser -k %1/udp ; fuser -k %1/tcp', "fr": 'Impossible de lier le port local %1 pour le pont WSS. Un autre processus peut encore le détenir (openvpn orphelin). Sous Linux : fuser -k %1/udp ; fuser -k %1/tcp', "el": 'Δεν ήταν δυνατή η δέσμευση τοπικής θύρας %1 για τη γέφυρα WSS. Άλλη διεργασία μπορεί να την κατέχει. Σε Linux: fuser -k %1/udp ; fuser -k %1/tcp'},  # 131 Could not bind local port %1 for the WSS bridge. Another
    {"ru": 'Не удалось создать временный файл конфигурации.', "fr": 'Impossible de créer le fichier de configuration temporaire.', "el": 'Δεν ήταν δυνατή η δημιουργία προσωρινού αρχείου ρυθμίσεων.'},  # 132 Could not create temporary config file.
    {"ru": 'Мост UDP↔WSS на порту %1…', "fr": 'Pont UDP↔WSS sur le port %1…', "el": 'Γέφυρα UDP↔WSS στη θύρα %1…'},  # 133 UDP↔WSS bridge on port %1…
    {"ru": 'Мост TCP↔WSS на порту %1…', "fr": 'Pont TCP↔WSS sur le port %1…', "el": 'Γέφυρα TCP↔WSS στη θύρα %1…'},  # 134 TCP↔WSS bridge on port %1…
    {"ru": 'Запуск OpenVPN…', "fr": 'Démarrage d’OpenVPN…', "el": 'Εκκίνηση OpenVPN…'},  # 135 Starting OpenVPN…
    {"ru": 'OpenVPN не запустился за 8 с (%1). Выдайте CAP_NET_ADMIN бинарнику openvpn, если TUN блокируется.', "fr": 'OpenVPN n’a pas démarré en 8 s (%1). Accordez CAP_NET_ADMIN au binaire openvpn si TUN est bloqué.', "el": 'Το OpenVPN δεν ξεκίνησε εντός 8 δ (%1). Χορηγήστε CAP_NET_ADMIN στο δυαδικό openvpn αν αποκλείεται το TUN.'},  # 136 OpenVPN did not start within 8s (%1). Grant CAP_NET_ADMI
    {"ru": 'OpenVPN запущен.', "fr": 'OpenVPN a démarré.', "el": 'Το OpenVPN ξεκίνησε.'},  # 137 OpenVPN started.
    {"ru": 'IgnoreRedirectGateway включён: push маршрута по умолчанию с сервера игнорируется, публичный IP обычно не меняется, туннель используется только для VPN-маршрутов. Установите OpenVpn.IgnoreRedirectGateway в false в appsettings.json, если нужен полный туннель (весь трафик и публичный IP через VPN).', "fr": 'IgnoreRedirectGateway est activé : la route par défaut poussée par le serveur est ignorée, votre IP publique reste en principe la même et seuls les itinéraires VPN passent par le tunnel. Mettez OpenVpn.IgnoreRedirectGateway à false dans appsettings.json si vous voulez un tunnel complet (tout le trafic et l’IP publique via le VPN).', "el": 'Το IgnoreRedirectGateway είναι ενεργό: αγνοείται η προώθηση προεπιλεγμένης διαδρομής, η δημόσια IP συνήθως παραμένει ίδια και μόνο VPN-διαδρομές χρησιμοποιούν τη σήραγγα. Ορίστε OpenVpn.IgnoreRedirectGateway σε false στο appsettings.json αν χρειάζεστε πλήρη σήραγγα (όλη η κίνηση και η δημόσια IP μέσω VPN).'},  # 138 IgnoreRedirectGateway is on: the server default-route pu
    {"ru": 'OpenVPN завершился (код %1).', "fr": 'OpenVPN s’est terminé (code %1).', "el": 'Το OpenVPN τερματίστηκε (κωδικός %1).'},  # 139 OpenVPN exited (code %1).
    {"ru": 'OpenVPN завершился с ошибкой (%1), вывода нет. В Linux для TUN обычно нужен CAP_NET_ADMIN у openvpn (Настройки → Выдать право TUN, или sudo setcap cap_net_admin+ep /путь/к/openvpn).', "fr": 'OpenVPN a échoué (code %1) sans sortie capturée. Sous Linux, TUN nécessite en général CAP_NET_ADMIN sur openvpn (Paramètres → Accorder TUN, ou sudo setcap cap_net_admin+ep /chemin/vers/openvpn).', "el": 'Το OpenVPN απέτυχε (έξοδος %1) χωρίς καταγεγραμμένη έξοδο. Στο Linux, το TUN συνήθως χρειάζεται CAP_NET_ADMIN στο openvpn (Ρυθμίσεις → Χορήγηση TUN, ή sudo setcap cap_net_admin+ep /διαδρομή/στο/openvpn).'},  # 140 OpenVPN failed (exit %1) with no captured output. On Lin
    {"ru": 'Создание TUN отклонено (нужен CAP_NET_ADMIN у процесса openvpn, не у GUI).\n\nOpenVPN завершился с ошибкой (%1):\n%2', "fr": 'Création TUN refusée (CAP_NET_ADMIN requis sur le processus openvpn, pas sur l’interface).\n\nOpenVPN a échoué (code %1) :\n%2', "el": 'Η δημιουργία TUN απορρίφθηκε (CAP_NET_ADMIN απαιτείται στη διεργασία openvpn, όχι στο GUI).\n\nΤο OpenVPN απέτυχε (έξοδος %1):\n%2'},  # 141 TUN creation was denied (CAP_NET_ADMIN required on the o
    {"ru": 'Ошибка OpenVPN (код %1):\n%2', "fr": 'Échec OpenVPN (code %1) :\n%2', "el": 'Αποτυχία OpenVPN (έξοδος %1):\n%2'},  # 142 OpenVPN failed (exit %1): %2
    {"ru": 'Ошибка процесса OpenVPN: %1', "fr": 'Erreur du processus OpenVPN : %1', "el": 'Σφάλμα διεργασίας OpenVPN: %1'},  # 143 OpenVPN process error: %1
    {"ru": 'Файл appsettings.json не найден (рабочая каталог, папка с exe или путь AppConfig), либо пусты Api:BaseUrl / GoogleAuth:ClientId.\nСм. строку в журнале «AppConfig: loaded …» при DATAGATE_LOG=1 или сборке Debug.', "fr": 'appsettings.json introuvable (répertoire courant, dossier de l’exe ou chemin AppConfig), ou Api:BaseUrl / GoogleAuth:ClientId vides.\nVoir la ligne « AppConfig: loaded … » sur stderr avec DATAGATE_LOG=1 ou build Debug.', "el": 'Δεν βρέθηκε appsettings.json (cwd, φάκελος exe ή διαδρομή AppConfig), ή κενά Api:BaseUrl / GoogleAuth:ClientId.\nΔείτε stderr «AppConfig: loaded …» με DATAGATE_LOG=1 ή Debug build.'},  # 144 appsettings.json not found (cwd, folder with exe, or App
]
