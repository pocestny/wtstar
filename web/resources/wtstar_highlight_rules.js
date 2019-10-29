
define(function(require, exports, module) {
"use strict";

var oop = require("../lib/oop");
var TextHighlightRules = require("./text_highlight_rules").TextHighlightRules;

var HighlightRules = function() {
    var keywordControls = (
      "if|while|for|pardo|else|do|return"
    );
    
   var keywordMapper = this.$keywords = this.createKeywordMapper({
      "keyword.control" : keywordControls,
      "storage.type" : "int|float|void",
      "storage.modifier" : "type|input|output",
      "keyword.operator" : "size|dim",
      "variable.language": "_"
    },"identifier");

    this.$rules = {
      "start": [
           {
                token : "comment",
                regex : "//$",
                next : "start"
            }, {
                token : "comment",
                regex : "//",
                next : "singleLineComment"
            },
            {
                token : "comment", // multi line comment
                regex : "\\/\\*",
                next : "comment"
            },
            {
                token : "constant.numeric", 
                regex : "[+-]?\\d+(?:(?:\\.\\d*)?(?:[eE][+-]?\\d+)?)?\\b"
            },
            {
              token: "keyword.operator",
              regex:"@"
            },
            {
                token : "keyword", // pre-compiler directives
                regex : "#\\s*(?:include|mode)\\b",
                next  : "directive"
            },{
                token : keywordMapper,
                regex : "[a-zA-Z_$][a-zA-Z0-9_$]*"
            },{
                token : "keyword.operator",
                regex : /--|\+\+|&&|\|\||[*%\/+\-&\^|~!<>=]=?/
            }, {
              token : "punctuation.operator",
              regex : "\\?|\\:|\\,|\\;|\\."
            }, {
                token : "paren.lparen",
                regex : "[[({]"
            }, {
                token : "paren.rparen",
                regex : "[\\])}]"
            },{ 
                token : "text",
                regex : "\\s+"
            }


      ],
      "directive" : [
            {
                token : "constant.other",
                regex : '\\s*["](?:(?:\\\\.)|(?:[^"\\\\]))*?["]',
                next : "start"
            }, 
            {
                token : "keyword.other", 
                regex : '\\s*(?:(?:CREW)|(?:EREW)|(?:cCRCW))',
                next : "start"
            }, 
      ],
      "comment" : [
            {
                token : "comment", // closing comment
                regex : "\\*\\/",
                next : "start"
            }, {
                defaultToken : "comment"
            }
      ],
      "singleLineComment" : [
            {
                token : "comment",
                regex : /\\$/,
                next : "singleLineComment"
            }, {
                token : "comment",
                regex : /$/,
                next : "start"
            }, {
                defaultToken: "comment"
            }
      ]
    };

    this.normalizeRules();
};



oop.inherits(HighlightRules, TextHighlightRules);

exports.HighlightRules = HighlightRules;
});
