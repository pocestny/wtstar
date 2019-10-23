export default 
function(hljs) {
  return {
    keywords: {
      keyword: 'input output type if else while pardo do while for',
      built_in: 'sqrt sqrtf log logf size sort int float char void dim'
    },
    contains: [
      {
        className: 'meta',
        begin:'#mode|cCRCW|CREW|EREW|#include|once'
      },
      {
        className: 'symbol',
        begin:'[=+-<>*&%\[\]{}();]'
      },
    {
      className: 'number',
      begin:'[0-9]*\.?[0-9]+([eE][-+]?[0-9]}+)?'
    },
      hljs.C_LINE_COMMENT_MODE,
      hljs.C_BLOCK_COMMENT_MODE
    ]
  };

};
