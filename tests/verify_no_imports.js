// ============================================================================
// Fichier: tests/verify_no_imports.js
// Version: 1.0.0
// Date: 2025-10-13
// ============================================================================
// Description:
//   Script de vérification pour s'assurer qu'aucun import ES6 n'est présent
//   dans les fichiers JavaScript du frontend.
//
// Usage:
//   node tests/verify_no_imports.js
//
// Vérifie:
//   - Absence d'imports ES6 (import ... from ...)
//   - Présence des exports globaux (window.X)
//   - Ordre de chargement dans index.html
// ============================================================================

const fs = require('fs');
const path = require('path');

// ============================================================================
// CONFIGURATION
// ============================================================================

const FRONTEND_DIR = path.join(__dirname, '../frontend');

const CRITICAL_FILES = [
    'js/core/BaseController.js',
    'js/core/BaseView.js',
    'js/core/BaseModel.js',
    'js/core/EventBus.js',
    'js/utils/Logger.js',
    'js/utils/Constants.js',
    'js/utils/Formatter.js',
    'js/services/BackendService.js'
];

const CONTROLLERS = [
    'js/controllers/EditorController.js',
    'js/controllers/HomeController.js',
    'js/controllers/PlaylistController.js',
    'js/controllers/RoutingController.js',
    'js/controllers/FileController.js'
];

const MODELS = [
    'js/models/FileModel.js',
    'js/models/PlaylistModel.js',
    'js/models/InstrumentModel.js',
    'js/models/EditorModel.js'
];

// Patterns à détecter
const PATTERNS = {
    es6Import: /^import\s+.*\s+from\s+['"].*['"]/gm,
    es6ExportDefault: /^export\s+default\s+/gm,
    es6ExportNamed: /^export\s+(const|let|var|class|function)/gm,
    windowExport: /window\.\w+\s*=\s*\w+/g,
    moduleExport: /module\.exports\s*=\s*\w+/g
};

// ============================================================================
// FONCTIONS UTILITAIRES
// ============================================================================

/**
 * Lit un fichier
 * @param {string} filePath - Chemin du fichier
 * @returns {string|null}
 */
function readFile(filePath) {
    const fullPath = path.join(FRONTEND_DIR, filePath);
    
    if (!fs.existsSync(fullPath)) {
        return null;
    }
    
    return fs.readFileSync(fullPath, 'utf8');
}

/**
 * Vérifie la présence d'imports ES6
 * @param {string} content - Contenu du fichier
 * @returns {Array<string>}
 */
function findES6Imports(content) {
    const matches = content.match(PATTERNS.es6Import);
    return matches || [];
}

/**
 * Vérifie la présence d'exports ES6
 * @param {string} content - Contenu du fichier
 * @returns {Array<string>}
 */
function findES6Exports(content) {
    const defaultExports = content.match(PATTERNS.es6ExportDefault) || [];
    const namedExports = content.match(PATTERNS.es6ExportNamed) || [];
    return [...defaultExports, ...namedExports];
}

/**
 * Vérifie la présence d'exports globaux
 * @param {string} content - Contenu du fichier
 * @returns {boolean}
 */
function hasWindowExport(content) {
    return PATTERNS.windowExport.test(content);
}

/**
 * Vérifie la présence d'exports module
 * @param {string} content - Contenu du fichier
 * @returns {boolean}
 */
function hasModuleExport(content) {
    return PATTERNS.moduleExport.test(content);
}

// ============================================================================
// VÉRIFICATION PRINCIPALE
// ============================================================================

/**
 * Vérifie un fichier
 * @param {string} filePath - Chemin du fichier
 * @returns {Object}
 */
function verifyFile(filePath) {
    const content = readFile(filePath);
    
    if (!content) {
        return {
            filePath,
            exists: false,
            valid: false,
            errors: ['File not found']
        };
    }
    
    const errors = [];
    const warnings = [];
    
    // Vérifier imports ES6
    const imports = findES6Imports(content);
    if (imports.length > 0) {
        errors.push(`Found ${imports.length} ES6 import(s):`);
        imports.forEach(imp => errors.push(`  - ${imp.trim()}`));
    }
    
    // Vérifier exports ES6
    const exports = findES6Exports(content);
    if (exports.length > 0) {
        errors.push(`Found ${exports.length} ES6 export(s):`);
        exports.forEach(exp => errors.push(`  - ${exp.trim()}`));
    }
    
    // Vérifier export window
    if (!hasWindowExport(content)) {
        warnings.push('No window.X export found');
    }
    
    // Vérifier export module
    if (!hasModuleExport(content)) {
        warnings.push('No module.exports found (Node.js compatibility)');
    }
    
    return {
        filePath,
        exists: true,
        valid: errors.length === 0,
        errors,
        warnings
    };
}

/**
 * Vérifie tous les fichiers
 */
function verifyAll() {
    console.log('🔍 VÉRIFICATION DES IMPORTS ES6\n');
    console.log('='.repeat(80));
    console.log('\n');
    
    let totalFiles = 0;
    let validFiles = 0;
    let invalidFiles = 0;
    let missingFiles = 0;
    
    const allFiles = [
        ...CRITICAL_FILES,
        ...CONTROLLERS,
        ...MODELS
    ];
    
    console.log(`📁 Fichiers à vérifier: ${allFiles.length}\n`);
    
    // Vérifier chaque fichier
    allFiles.forEach(filePath => {
        totalFiles++;
        
        const result = verifyFile(filePath);
        
        if (!result.exists) {
            console.log(`❌ ${filePath}`);
            console.log(`   → File not found\n`);
            missingFiles++;
            return;
        }
        
        if (result.valid) {
            console.log(`✅ ${filePath}`);
            if (result.warnings.length > 0) {
                result.warnings.forEach(w => console.log(`   ⚠️  ${w}`));
            }
            console.log('');
            validFiles++;
        } else {
            console.log(`❌ ${filePath}`);
            result.errors.forEach(e => console.log(`   → ${e}`));
            console.log('');
            invalidFiles++;
        }
    });
    
    // Résumé
    console.log('='.repeat(80));
    console.log('\n📊 RÉSUMÉ\n');
    console.log(`Total:    ${totalFiles} fichiers`);
    console.log(`✅ Valides: ${validFiles}`);
    console.log(`❌ Invalides: ${invalidFiles}`);
    console.log(`⚠️  Manquants: ${missingFiles}`);
    console.log('');
    
    if (invalidFiles === 0 && missingFiles === 0) {
        console.log('🎉 SUCCÈS: Aucun import ES6 détecté!\n');
        return 0;
    } else {
        console.log('❌ ÉCHEC: Des imports ES6 ont été détectés ou des fichiers sont manquants.\n');
        console.log('Actions à entreprendre:');
        console.log('1. Remplacer les imports ES6 par des variables globales');
        console.log('2. Ajouter window.X = X à la fin des fichiers');
        console.log('3. Vérifier l\'ordre de chargement dans index.html\n');
        return 1;
    }
}

/**
 * Vérifie l'ordre dans index.html
 */
function verifyIndexHTML() {
    console.log('='.repeat(80));
    console.log('\n📄 VÉRIFICATION DE index.html\n');
    
    const indexPath = path.join(FRONTEND_DIR, 'index.html');
    
    if (!fs.existsSync(indexPath)) {
        console.log('❌ index.html not found\n');
        return 1;
    }
    
    const content = fs.readFileSync(indexPath, 'utf8');
    
    // Extraire l'ordre des scripts
    const scriptRegex = /<script\s+src="([^"]+)"><\/script>/g;
    const scripts = [];
    let match;
    
    while ((match = scriptRegex.exec(content)) !== null) {
        scripts.push(match[1]);
    }
    
    console.log(`Nombre de scripts: ${scripts.length}\n`);
    
    // Vérifier l'ordre critique
    const criticalOrder = [
        'js/core/EventBus.js',
        'js/utils/Logger.js',
        'js/core/BaseModel.js',
        'js/core/BaseView.js',
        'js/core/BaseController.js',
        'js/services/BackendService.js'
    ];
    
    let orderValid = true;
    let lastIndex = -1;
    
    criticalOrder.forEach(file => {
        const index = scripts.indexOf(file);
        
        if (index === -1) {
            console.log(`⚠️  ${file} - Non trouvé`);
            orderValid = false;
        } else if (index < lastIndex) {
            console.log(`❌ ${file} - Ordre incorrect (devrait être après le précédent)`);
            orderValid = false;
        } else {
            console.log(`✅ ${file} - Position ${index + 1}`);
            lastIndex = index;
        }
    });
    
    console.log('');
    
    if (orderValid) {
        console.log('✅ Ordre de chargement correct\n');
        return 0;
    } else {
        console.log('❌ Ordre de chargement incorrect\n');
        return 1;
    }
}

// ============================================================================
// EXÉCUTION
// ============================================================================

function main() {
    const result1 = verifyAll();
    const result2 = verifyIndexHTML();
    
    process.exit(result1 || result2);
}

main();
