// Formula input component utilities

class FormulaInputHelper {
    static validateSyntax(formula) {
        // Basic syntax validation for CNF formulas
        const errors = [];

        // Check for balanced parentheses
        let parenCount = 0;
        for (let char of formula) {
            if (char === '(') parenCount++;
            if (char === ')') parenCount--;
            if (parenCount < 0) {
                errors.push('Unbalanced parentheses');
                break;
            }
        }
        if (parenCount !== 0) {
            errors.push('Unbalanced parentheses');
        }

        // Check for valid operators
        const validPattern = /^[\w\s\(\)\|\&\~\¬]+$/;
        if (!validPattern.test(formula)) {
            errors.push('Invalid characters in formula');
        }

        return {
            valid: errors.length === 0,
            errors
        };
    }

    static formatFormula(formula) {
        // Normalize formula formatting
        return formula
            .replace(/\s+/g, ' ')  // Normalize whitespace
            .replace(/\s*\|\s*/g, ' | ')  // Space around |
            .replace(/\s*\&\s*/g, ' & ')  // Space around &
            .trim();
    }

    static getFormulaInfo(formula) {
        // Extract basic information about the formula
        const clauses = formula.split('&').map(c => c.trim());
        const variables = new Set();

        clauses.forEach(clause => {
            const varPattern = /[~¬]?([a-zA-Z_]\w*)/g;
            let match;
            while ((match = varPattern.exec(clause)) !== null) {
                variables.add(match[1]);
            }
        });

        return {
            numClauses: clauses.length,
            numVariables: variables.size,
            variables: Array.from(variables).sort()
        };
    }
}
