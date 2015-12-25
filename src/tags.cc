#include "tagnode.h"
#include "module.h"
#include "evalcontext.h"
#include "builtin.h"
#include "printutils.h"
#include <sstream>
#include <assert.h>
#include <boost/unordered/unordered_map.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/assign/std/vector.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/foreach.hpp>

using namespace boost::assign; // bring 'operator+=()' into scope

class TagModule : public AbstractModule
{
public:
	TagModule(int type);
	virtual ~TagModule();
	virtual AbstractNode *instantiate(const Context *ctx, const ModuleInstantiation *inst, EvalContext *evalctx) const;
	
private:
	int type;
};

TagModule::TagModule(int type_) 
{
	type = type_;
}

TagModule::~TagModule()
{
}

void selectByTag(std::vector<AbstractNode *>* result, std::vector<AbstractNode *> startingSet, std::set<std::string> filter) {
	BOOST_FOREACH(AbstractNode* child, startingSet) {
		TagNode* tag = dynamic_cast<TagNode*>( child );
		if(!tag || tag->type != 0) {
			selectByTag(result, child->children, filter);
			continue;
		}
		std::list<std::string> missing_tags;
		std::set_difference(filter.begin(), filter.end(), tag->tags.begin(), tag->tags.end(), missing_tags.begin());
		if(!missing_tags.empty()) continue;
		result->push_back(child);
	}
}

AbstractNode *TagModule::instantiate(const Context *ctx, const ModuleInstantiation *inst, EvalContext *evalctx) const
{
	TagNode *node = new TagNode(inst);
	node->type = type;

	AssignmentList args;
	args += Assignment("set");

	Context c(ctx);
	c.setVariables(args, evalctx);
	inst->scope.apply(*evalctx);

	ValuePtr v = c.lookup_variable("set");
	if (v->type() == Value::STRING) {
		std::string tags = v->toString();
		std::istringstream f(tags);
		std::string s;
		while (getline(f, s, ',')) {
			node->tags.insert(s);
		}
	}

	std::vector<AbstractNode *> instantiatednodes = inst->instantiateChildren(evalctx);
	if(type == 1) {
		// do the filtering
		std::vector<AbstractNode *> filtered;
		selectByTag(&filtered, instantiatednodes, node->tags);
		instantiatednodes = filtered;
	}
	
	node->children.insert(node->children.end(), instantiatednodes.begin(), instantiatednodes.end());

	return node;
}

std::string TagNode::toString() const
{
	std::stringstream stream;

	stream << name();
	stream << "(\"" << boost::algorithm::join(tags,", ") << "\")";

	return stream.str();
}

std::string TagNode::name() const
{
	if(type == 0)
		return "tag";
	else
		return "tagsearch";
}

void register_builtin_tags()
{
	Builtins::init("tag", new TagModule(0));
	Builtins::init("tagsearch", new TagModule(1));
}
