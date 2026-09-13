/**
 * Copyright (c) 2026 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */
if (typeof window !== 'undefined') {
    Module['preRun'] = Module['preRun'] || [];
    Module['preRun'].push(function() {
        addRunDependency('world-storage');
        var storage = Module.worldStorage = {
            ready: false,
            busy: false,
            pending: null,
            db: null,
            error: false
        };
        var notice = document.createElement('div');
        notice.style.cssText = 'position:fixed;bottom:12px;left:12px;padding:8px;background:#222;color:white;font:14px sans-serif;display:none';
        notice.setAttribute('role', 'status');
        document.body.appendChild(notice);

        function show(message) {
            notice.textContent = message;
            notice.style.display = message ? 'block' : 'none';
        }

        function flush() {
            if (storage.busy || !storage.pending) return;
            storage.busy = true;
            var snapshot = storage.pending;
            storage.pending = null;
            show('Saving world in browser…');
            var transaction;
            function failed(error) {
                console.error('World save failed', error);
                storage.busy = false;
                storage.error = true;
                // Keep the newest snapshot available for retry.
                if (!storage.pending) storage.pending = snapshot;
                show('Browser save failed. Keep this page open while storage is unavailable.');
                setTimeout(flush, 5000);
            }
            try {
                transaction = storage.db.transaction('worlds', 'readwrite');
                transaction.objectStore('worlds').put(snapshot, 'world');
            } catch (error) {
                failed(error);
                return;
            }
            transaction.oncomplete = function() {
                storage.busy = false;
                storage.error = false;
                show('');
                flush();
            };
            transaction.onabort = function() { failed(transaction.error); };
            transaction.onerror = function(event) { event.preventDefault(); };
        }

        storage.save = function() {
            if (!storage.ready) return false;
            try {
                // Called with the C database mutex held. readFile makes an owned
                // copy before any asynchronous IndexedDB work starts.
                storage.pending = FS.readFile('/world/world.sqlite');
                flush();
                return true;
            } catch (error) {
                console.error(error);
                show('Could not copy the world for browser saving.');
                return false;
            }
        };

        window.addEventListener('beforeunload', function(event) {
            if (storage.busy || storage.pending || storage.error) {
                event.preventDefault();
                event.returnValue = '';
            }
        });

        function unavailable(error) {
            console.error('World storage unavailable', error);
            show('Single-player storage unavailable. Close other game tabs or enable browser storage.');
            removeRunDependency('world-storage');
        }

        function open() {
            var request = indexedDB.open('midless-worlds', 1);
            request.onupgradeneeded = function() {
                request.result.createObjectStore('worlds');
            };
            request.onerror = function() { unavailable(request.error); };
            request.onsuccess = function() {
                storage.db = request.result;
                var transaction = storage.db.transaction('worlds', 'readonly');
                var read = transaction.objectStore('worlds').get('world');
                read.onsuccess = function() {
                    try {
                        FS.mkdirTree('/world');
                        if (read.result) FS.writeFile('/world/world.sqlite', read.result);
                    } catch (error) {
                        transaction.abort();
                    }
                };
                transaction.oncomplete = function() {
                    storage.ready = true;
                    removeRunDependency('world-storage');
                };
                transaction.onabort = function() { unavailable(transaction.error); };
            };
        }

        // A single owner prevents independent tabs overwriting the same world.
        if (!navigator.locks) {
            unavailable(new Error('Web Locks requires a secure context'));
        } else {
            navigator.locks.request('midless-world', {ifAvailable: true}, function(lock) {
                if (!lock) {
                    unavailable(new Error('World is open in another tab'));
                    return;
                }
                try { open(); } catch (error) { unavailable(error); }
                return new Promise(function() {});
            }).catch(unavailable);
        }
    });
}
