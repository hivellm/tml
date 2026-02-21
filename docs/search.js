// TML Documentation Search
(function() {
    const searchInput = document.getElementById('search-input');
    const searchResults = document.getElementById('search-results');
    let selectedIndex = -1;
    let currentResults = [];

    if (!searchInput || !searchResults || !window.searchIndex) return;

    function escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    function getKindClass(kind) {
        const kindMap = {
            'function': 'function',
            'method': 'method',
            'struct': 'struct',
            'enum': 'enum',
            'behavior': 'behavior',
            'trait': 'behavior',
            'constant': 'constant',
            'field': 'field'
        };
        return kindMap[kind] || 'function';
    }

    function search(query) {
        if (!query.trim()) {
            searchResults.classList.remove('active');
            return [];
        }

        const q = query.toLowerCase();
        const results = window.searchIndex.filter(item => {
            const name = (item.name || '').toLowerCase();
            const path = (item.path || '').toLowerCase();
            return name.includes(q) || path.includes(q);
        }).slice(0, 15);

        return results;
    }

    function renderResults(results) {
        if (results.length === 0) {
            searchResults.innerHTML = '<div class="search-empty">No results found</div>';
            searchResults.classList.add('active');
            return;
        }

        searchResults.innerHTML = results.map((item, index) => `
            <a href="${item.module || 'index'}.html#${item.id || item.name}"
               class="search-result-item ${index === selectedIndex ? 'selected' : ''}"
               data-index="${index}">
                <span class="result-kind ${getKindClass(item.kind)}">${escapeHtml(item.kind)}</span>
                <div class="result-info">
                    <div class="result-name">${escapeHtml(item.name)}</div>
                    <div class="result-path">${escapeHtml(item.path || '')}</div>
                </div>
            </a>
        `).join('');
        searchResults.classList.add('active');
    }

    function updateSelection() {
        const items = searchResults.querySelectorAll('.search-result-item');
        items.forEach((item, index) => {
            item.classList.toggle('selected', index === selectedIndex);
        });
        if (selectedIndex >= 0 && items[selectedIndex]) {
            items[selectedIndex].scrollIntoView({ block: 'nearest' });
        }
    }

    searchInput.addEventListener('input', (e) => {
        selectedIndex = -1;
        currentResults = search(e.target.value);
        renderResults(currentResults);
    });

    searchInput.addEventListener('keydown', (e) => {
        const items = searchResults.querySelectorAll('.search-result-item');

        if (e.key === 'ArrowDown') {
            e.preventDefault();
            selectedIndex = Math.min(selectedIndex + 1, items.length - 1);
            updateSelection();
        } else if (e.key === 'ArrowUp') {
            e.preventDefault();
            selectedIndex = Math.max(selectedIndex - 1, -1);
            updateSelection();
        } else if (e.key === 'Enter') {
            e.preventDefault();
            if (selectedIndex >= 0 && items[selectedIndex]) {
                items[selectedIndex].click();
            }
        } else if (e.key === 'Escape') {
            searchResults.classList.remove('active');
            searchInput.blur();
        }
    });

    // Global shortcut: / to focus search
    document.addEventListener('keydown', (e) => {
        if (e.key === '/' && document.activeElement !== searchInput) {
            e.preventDefault();
            searchInput.focus();
        }
    });

    // Close on outside click
    document.addEventListener('click', (e) => {
        if (!searchInput.contains(e.target) && !searchResults.contains(e.target)) {
            searchResults.classList.remove('active');
        }
    });

    // Mobile toggle
    const mobileToggle = document.querySelector('.mobile-toggle');
    const sidebar = document.querySelector('.sidebar');
    if (mobileToggle && sidebar) {
        mobileToggle.addEventListener('click', () => {
            sidebar.classList.toggle('open');
        });
    }
})();

// Toggle modules list expand/collapse
function toggleModulesList() {
    const list = document.getElementById('modules-list');
    const btn = list.parentElement.querySelector('.nav-toggle');
    if (list && btn) {
        list.classList.toggle('expanded');
        btn.textContent = list.classList.contains('expanded') ? 'Show less' : 'Show all modules';
    }
}
