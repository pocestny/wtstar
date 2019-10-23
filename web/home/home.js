import './home.scss'
import wt from './wt.js'
import { el, mount, setAttr } from "redom"
import{library, icon} from '@fortawesome/fontawesome-svg-core'
import {faBook,faBookOpen} from '@fortawesome/free-solid-svg-icons'
library.add(faBook,faBookOpen)

if (typeof hljs!=='undefined') {
  hljs.registerLanguage("wt",wt);
  hljs.initHighlightingOnLoad();
}

window.addEventListener('load', () => {
  document.querySelectorAll('.codepre').forEach((item)=>{ 
    if (typeof(item.dataset.id)!=='undefined'){
    var butt = el("span","try me");
    mount(item,butt,item.firstChild);
    setAttr(butt, { 
      class:"banner" ,
      style:
      {
        'position': 'absolute',
        'top':'10px',
        'right':'10px',
        'width':'60px',
        'border-radius':'5px',
        'cursor':'pointer'
      },
      onclick:()=>window.open('./ide/'+item.dataset.id,'_blank')
    });

  }});}
);

document.addEventListener('DOMContentLoaded', () => {
  // mount icons
  document.querySelectorAll('.has-icon').forEach((el) => {
    var prefix = 'fas';
    var iconName = el.dataset.iconName;
    var size = el.dataset.iconSize;
    if (el.dataset.iconPrefix) prefix=el.dataset.iconPrefix;
    el.appendChild(icon({'prefix' : prefix, 'iconName' : iconName}, {
                transform : {'size' : size}
              }).node[0]);
  });


  const $navbarBurgers = Array.prototype.slice.call(
      document.querySelectorAll('.navbar-burger'), 0);

  if ($navbarBurgers.length > 0) {
    $navbarBurgers.forEach(el => {
      el.addEventListener('click', () => {
        const target = el.dataset.target;
        const $target = document.getElementById(target);
        el.classList.toggle('is-active');
        $target.classList.toggle('is-active');
      });
    });
  }
});
