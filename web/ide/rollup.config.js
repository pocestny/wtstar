import scss from 'rollup-plugin-scss'
import alias from 'rollup-plugin-alias'
import { string } from "rollup-plugin-string"

module.exports = {
  input: './main.js',
  output: {
    file: '../../_build/web/ide.js',
    format:'iife'
  },
  plugins:[
    scss({output:true}),
    string({include:'**/*.txt'}),
    alias({entries:[
        {find:'backend.js', replacement: '../../_build/web/backend.js'}
    ]})
  ]
};
