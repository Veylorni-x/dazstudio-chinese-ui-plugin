// SPDX-License-Identifier: MIT
// Translate a graphics item's rendered copy, leaving editable/source text intact.
class GraphicsTextEffect final:public QGraphicsEffect {
    QPointer<Dictionary> dictionary;
    QGraphicsTextItem *rich=nullptr;
    QGraphicsSimpleTextItem *plain=nullptr;
    QPointer<QTextDocument> sourceDocument;
    std::unique_ptr<QTextDocument> rendered;
    QString sourceText;
    QFont sourceFont;
    int revision=-1;
    bool dirty=true;
public:
    GraphicsTextEffect(Dictionary*d,QGraphicsTextItem*item):dictionary(d),rich(item){}
    GraphicsTextEffect(Dictionary*d,QGraphicsSimpleTextItem*item):dictionary(d),plain(item){}
    QRectF boundingRectFor(const QRectF&r)const override{return r.adjusted(0,-2,0,2);}
protected:
    void draw(QPainter*painter)override{
        if(!dictionary||!dictionary->enabled||(rich&&(rich->textInteractionFlags()&Qt::TextEditable))){drawSource(painter);return;}
        QFont font=rich?rich->font():plain->font();
        if(rich&&sourceDocument!=rich->document()){
            sourceDocument=rich->document();dirty=true;
            connect(sourceDocument.data(),&QTextDocument::contentsChanged,this,[this]{dirty=true;update();});
        }
        QString raw=plain?plain->text():QString();
        if(dirty||revision!=dictionary->revision||sourceFont!=font||(plain&&raw!=sourceText)){
            QString original=rich?rich->toHtml():raw;
            QString target=rich?dictionary->richText(original):dictionary->display(original,"GraphicsText");
            sourceText=raw;sourceFont=font;revision=dictionary->revision;dirty=false;
            rendered.reset();
            if(target!=original){
                if(rich){rendered.reset(rich->document()->clone());rendered->setHtml(target);}
                else{rendered=std::make_unique<QTextDocument>();rendered->setDocumentMargin(0);rendered->setDefaultFont(font);rendered->setPlainText(target);}
            }
        }
        if(!rendered){drawSource(painter);return;}
        auto bounds=sourceBoundingRect(Qt::LogicalCoordinates);
        painter->save();painter->setClipRect(bounds.adjusted(0,-2,0,2),Qt::IntersectClip);
        QAbstractTextDocumentLayout::PaintContext context;
        context.palette.setColor(QPalette::Text,rich?rich->defaultTextColor():plain->brush().color());
        rendered->documentLayout()->draw(painter,context);painter->restore();
    }
};
