/* Prism TML language definition — hand-authored from the TML spec.
 * Covers: keywords, built-in types, comments, strings, numbers,
 * decorators (`@foo`), template literals (`` ` ``), punctuation.
 * Apache-2.0 (TML project).
 */
(function (Prism) {
  if (!Prism) return;
  Prism.languages.tml = {
    "doc-comment": {
      pattern: /\/\/[!\/].*/,
      greedy: true,
      alias: "comment"
    },
    "comment": {
      pattern: /\/\/.*|\/\*[\s\S]*?\*\//,
      greedy: true
    },
    "decorator": {
      pattern: /@[A-Za-z_][\w]*/,
      alias: "function"
    },
    "template-string": {
      pattern: /`(?:\\.|\$\{(?:[^{}]|\{[^}]*\})*\}|[^\\`])*`/,
      greedy: true,
      inside: {
        "interpolation": {
          pattern: /\{[^}]*\}/,
          inside: {
            "interpolation-punctuation": {
              pattern: /^\{|\}$/,
              alias: "punctuation"
            },
            rest: null // filled below to avoid forward reference
          }
        },
        "string": /[\s\S]+?/
      }
    },
    "string": {
      pattern: /"(?:\\.|[^\\"])*"/,
      greedy: true
    },
    "keyword": /\b(?:pub|func|let|var|type|enum|behavior|impl|use|mod|when|if|else|for|in|to|through|loop|while|return|break|continue|and|or|not|as|lowlevel|match)\b/,
    "builtin-type": {
      pattern: /\b(?:I8|I16|I32|I64|U8|U16|U32|U64|F32|F64|Bool|Str|Text|List|HashMap|HashSet|Maybe|Outcome|Heap|Ref|RawPtr|Buffer|Byte|Void)\b/,
      alias: "class-name"
    },
    "literal": /\b(?:true|false|null|Nothing)\b/,
    "number": /\b(?:0x[0-9a-fA-F_]+|0b[01_]+|[0-9][\d_]*(?:\.[\d_]+)?(?:[eE][+-]?\d+)?)\b/,
    "operator": /(?:[=!<>]=?|[+\-*\/%&|^~]=?|[<>]<|\?\.?|::|->|=>|\.\.?|[?!])/,
    "punctuation": /[\[\](){},;:]/
  };
  // Fix forward reference (template-string ↪ interpolation ↪ rest)
  Prism.languages.tml["template-string"].inside.interpolation.inside.rest = Prism.languages.tml;
})(window.Prism);
