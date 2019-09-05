import scss from 'rollup-plugin-scss'
import { string } from "rollup-plugin-string";

module.exports = {
  input: './main.js',
  output: {
    file: './index.js',
    format: 'iife'
  },
  plugins:[
    scss({output:'./index.css'}),
    string({include:'**/*.txt'})
  ]
};
