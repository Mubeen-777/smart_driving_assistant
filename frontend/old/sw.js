const CACHE_NAME = 'smart-drive-v1.2.0';
const urlsToCache = [
    '/',
    '/index.html',
    '/login.html',
    '/style.css',
    '/app.js',
    '/database.js',
    '/camera.js',
    '/analytics.js',
    '/modals.js',
    '/main.js',
    'https:
    'https:
];

self.addEventListener('install', event => {
    event.waitUntil(
        caches.open(CACHE_NAME)
            .then(cache => {
                console.log('📦 Caching app shell');
                return cache.addAll(urlsToCache);
            })
            .then(() => self.skipWaiting())
    );
});

self.addEventListener('activate', event => {
    event.waitUntil(
        caches.keys().then(cacheNames => {
            return Promise.all(
                cacheNames.map(cacheName => {
                    if (cacheName !== CACHE_NAME) {
                        console.log('🗑️ Deleting old cache:', cacheName);
                        return caches.delete(cacheName);
                    }
                })
            );
        }).then(() => self.clients.claim())
    );
});

self.addEventListener('fetch', event => {
    
    if (event.request.url.startsWith('ws:
        return;
    }
    
    if (event.request.url.includes('localhost:8080')) {
        return;
    }
    
    event.respondWith(
        caches.match(event.request)
            .then(response => {
                if (response) {
                    return response;
                }
                
                return fetch(event.request)
                    .then(response => {
                        
                        if (!response || response.status !== 200 || response.type !== 'basic') {
                            return response;
                        }
                        
                        const responseToCache = response.clone();
                        
                        caches.open(CACHE_NAME)
                            .then(cache => {
                                cache.put(event.request, responseToCache);
                            });
                        
                        return response;
                    })
                    .catch(() => {
                        
                        if (event.request.mode === 'navigate') {
                            return caches.match('/index.html');
                        }
                        
                        return caches.match(event.request);
                    });
            })
    );
});

self.addEventListener('sync', event => {
    if (event.tag === 'sync-trips') {
        event.waitUntil(syncTrips());
    } else if (event.tag === 'sync-expenses') {
        event.waitUntil(syncExpenses());
    }
});

async function syncTrips() {
    
    const trips = await getOfflineData('trips');
    
    for (const trip of trips) {
        try {
            
            await fetch('http:
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(trip)
            });
            
            await removeOfflineData('trips', trip.id);
        } catch (error) {
            console.error('Failed to sync trip:', error);
        }
    }
}

async function syncExpenses() {
    
    const expenses = await getOfflineData('expenses');
    
    for (const expense of expenses) {
        try {
            
            await fetch('http:
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(expense)
            });
            
            await removeOfflineData('expenses', expense.id);
        } catch (error) {
            console.error('Failed to sync expense:', error);
        }
    }
}

function openDB() {
    return new Promise((resolve, reject) => {
        const request = indexedDB.open('SmartDriveDB', 1);
        
        request.onerror = () => reject(request.error);
        request.onsuccess = () => resolve(request.result);
        
        request.onupgradeneeded = (event) => {
            const db = event.target.result;
            
            if (!db.objectStoreNames.contains('offlineData')) {
                const store = db.createObjectStore('offlineData', { keyPath: 'id' });
                store.createIndex('type', 'type', { unique: false });
            }
        };
    });
}

async function getOfflineData(type) {
    const db = await openDB();
    return new Promise((resolve, reject) => {
        const transaction = db.transaction(['offlineData'], 'readonly');
        const store = transaction.objectStore('offlineData');
        const index = store.index('type');
        const request = index.getAll(type);
        
        request.onerror = () => reject(request.error);
        request.onsuccess = () => resolve(request.result);
    });
}

async function removeOfflineData(type, id) {
    const db = await openDB();
    return new Promise((resolve, reject) => {
        const transaction = db.transaction(['offlineData'], 'readwrite');
        const store = transaction.objectStore('offlineData');
        const request = store.delete(id);
        
        request.onerror = () => reject(request.error);
        request.onsuccess = () => resolve();
    });
}

self.addEventListener('push', event => {
    if (!event.data) return;
    
    const data = event.data.json();
    
    const options = {
        body: data.body,
        icon: '/icon-192.png',
        badge: '/badge-72.png',
        vibrate: [100, 50, 100],
        data: {
            dateOfArrival: Date.now(),
            primaryKey: 1,
            url: data.url || '/'
        },
        actions: [
            {
                action: 'view',
                title: 'View'
            },
            {
                action: 'close',
                title: 'Close'
            }
        ]
    };
    
    event.waitUntil(
        self.registration.showNotification(data.title, options)
    );
});

self.addEventListener('notificationclick', event => {
    event.notification.close();
    
    if (event.action === 'view') {
        event.waitUntil(
            clients.openWindow(event.notification.data.url)
        );
    }
});